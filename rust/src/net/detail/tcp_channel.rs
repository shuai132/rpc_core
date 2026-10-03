use std::cell::{Cell, RefCell};
use std::collections::VecDeque;
use std::rc::Rc;

use log::{debug, trace};
use tokio::io::{AsyncRead, AsyncReadExt, AsyncWriteExt, ReadHalf};
use tokio::net::TcpStream;
use tokio::select;
use tokio::sync::Notify;

use crate::net::config::TcpConfig;

pub struct TcpChannel {
    config: Rc<RefCell<TcpConfig>>,
    on_close: RefCell<Option<Rc<dyn Fn()>>>,
    on_data: RefCell<Option<Rc<dyn Fn(Vec<u8>)>>>,
    send_queue: RefCell<VecDeque<Vec<u8>>>,
    send_buffer_size: Cell<usize>,
    send_queue_notify: Notify,
    quit_notify: Notify,
    close_finish_notify: Notify,
    active_loops: Cell<u8>,
    is_open: RefCell<bool>,
}

impl TcpChannel {
    pub fn new(config: Rc<RefCell<TcpConfig>>) -> Rc<Self> {
        Rc::new(Self {
            config,
            on_close: None.into(),
            on_data: None.into(),
            send_queue: VecDeque::new().into(),
            send_buffer_size: Cell::new(0),
            send_queue_notify: Notify::new(),
            quit_notify: Notify::new(),
            close_finish_notify: Notify::new(),
            active_loops: Cell::new(0),
            is_open: false.into(),
        })
    }

    pub fn is_open(&self) -> bool {
        *self.is_open.borrow()
    }

    pub fn on_data<F>(&self, callback: F)
    where
        F: Fn(Vec<u8>) + 'static,
    {
        let previous = self.on_data.borrow_mut().replace(Rc::new(callback));
        drop(previous);
    }

    pub fn on_close<F>(&self, callback: F)
    where
        F: Fn() + 'static,
    {
        let previous = self.on_close.borrow_mut().replace(Rc::new(callback));
        drop(previous);
    }

    pub fn send(&self, data: Vec<u8>) -> bool {
        if !*self.is_open.borrow() {
            debug!("channel not connected");
            return false;
        }
        let (auto_pack, limit) = {
            let config = self.config.borrow();
            (config.auto_pack, config.max_send_buffer_size as usize)
        };
        if auto_pack && data.len() > u32::MAX as usize {
            return false;
        }
        if data.is_empty() && !auto_pack {
            debug!("send empty ignore");
            return false;
        }
        let size = data.len().checked_add(if auto_pack { 4 } else { 0 });
        let Some(total) = size.and_then(|size| self.send_buffer_size.get().checked_add(size))
        else {
            return false;
        };
        if limit != 0 && total > limit {
            return false;
        }
        self.send_buffer_size.set(total);
        {
            let mut send_queue = self.send_queue.borrow_mut();
            if auto_pack {
                send_queue.push_back((data.len() as u32).to_le_bytes().to_vec());
            }
            if !data.is_empty() {
                send_queue.push_back(data);
            }
        }
        self.send_queue_notify.notify_one();
        true
    }

    pub fn send_str(&self, data: impl ToString) {
        self.send(data.to_string().as_bytes().to_vec());
    }

    pub fn close(&self) {
        self.do_close();
    }

    pub async fn wait_close_finish(&self) {
        while self.active_loops.get() != 0 {
            self.close_finish_notify.notified().await;
        }
    }

    // Reopening requires close() followed by wait_close_finish().
    pub fn do_open(self: &Rc<Self>, stream: TcpStream) {
        debug_assert_eq!(
            self.active_loops.get(),
            0,
            "wait for old IO before reopening"
        );
        self.active_loops.set(2);
        *self.is_open.borrow_mut() = true;
        // Own cleanup before spawning so it also runs on panic or an unpolled
        // task being dropped. Do not invoke user callbacks while unwinding.
        struct IoScope(Rc<TcpChannel>);
        impl Drop for IoScope {
            fn drop(&mut self) {
                self.0.do_close();
                self.0.finish_loop();
            }
        }
        let read_scope = IoScope(self.clone());
        let write_scope = IoScope(self.clone());
        let this = self.clone();
        tokio::task::spawn_local(async move {
            let (read_half, write_half) = tokio::io::split(stream);

            // read loop task
            if this.config.borrow().auto_pack {
                let this = this.clone();
                tokio::task::spawn_local(async move {
                    let _scope = read_scope;
                    let mut read_half = read_half;
                    loop {
                        if !*this.is_open.borrow() {
                            break;
                        }
                        select! {
                            goon = this.do_read_header(&mut read_half) => {
                                if !goon {
                                    break;
                                }
                            },
                            _ = this.quit_notify.notified() => {
                                break;
                            },
                        }
                    }
                    trace!("loop exit: read");
                });
            } else {
                let this = this.clone();
                tokio::task::spawn_local(async move {
                    let _scope = read_scope;
                    let mut read_half = read_half;
                    loop {
                        if !*this.is_open.borrow() {
                            break;
                        }
                        select! {
                            goon = this.do_read_data(&mut read_half) => {
                                if !goon {
                                    break;
                                }
                            },
                            _ = this.quit_notify.notified() => {
                                break;
                            },
                        }
                    }
                    trace!("loop exit: read");
                });
            }

            // write loop task
            tokio::task::spawn_local(async move {
                let _scope = write_scope;
                let mut write_half = write_half;
                loop {
                    if !*this.is_open.borrow() {
                        break;
                    }

                    let send_data = this.send_queue.borrow_mut().pop_front();
                    if let Some(data) = send_data {
                        let mut result = None;
                        select! {
                            write_ret = write_half.write_all(&data) => {
                                result = write_ret.ok();
                            },
                            _ = this.quit_notify.notified() => {},
                        }
                        if result.is_none() {
                            this.do_close();
                            break;
                        }
                        if this.is_open() {
                            this.send_buffer_size
                                .set(this.send_buffer_size.get() - data.len());
                        }
                    } else {
                        this.send_queue_notify.notified().await;
                    }
                }
                trace!("loop exit: write");

                let callback = this.on_close.borrow().clone();
                if let Some(on_close) = callback {
                    on_close();
                }
            });
        });
    }

    fn finish_loop(&self) {
        self.active_loops.set(self.active_loops.get() - 1);
        if self.active_loops.get() == 0 {
            self.close_finish_notify.notify_waiters();
        }
    }

    fn do_close(&self) {
        self.send_queue.borrow_mut().clear();
        self.send_buffer_size.set(0);
        if !*self.is_open.borrow() {
            return;
        }
        *self.is_open.borrow_mut() = false;
        self.quit_notify.notify_waiters();
        self.send_queue_notify.notify_one();
    }

    async fn do_read_data(&self, read_half: &mut ReadHalf<TcpStream>) -> bool {
        let mut buffer = vec![];
        let read_result = read_half.read_buf(&mut buffer).await.ok();

        if read_result.is_some() && !buffer.is_empty() {
            let callback = self.on_data.borrow().clone();
            if let Some(on_data) = callback {
                on_data(buffer);
            }
            true
        } else {
            self.do_close();
            false
        }
    }

    async fn do_read_header(&self, read_half: &mut ReadHalf<TcpStream>) -> bool {
        let mut buffer = [0u8; 4];
        let read_result = read_half.read_exact(&mut buffer).await.ok();

        if read_result.is_some() {
            let body_size = u32::from_le_bytes(buffer);
            self.do_read_body(read_half, body_size).await
        } else {
            self.do_close();
            false
        }
    }

    async fn do_read_body<R: AsyncRead + Unpin>(&self, read_half: &mut R, body_size: u32) -> bool {
        let limit = self.config.borrow().max_body_size;
        if limit != 0 && body_size > limit {
            self.do_close();
            return false;
        }
        // Grow only for bytes actually received, even when the caller opts out
        // of the frame limit. A length header alone must not reserve its body.
        let mut buffer = Vec::new();
        let mut chunk = [0u8; 8192];
        while buffer.len() < body_size as usize {
            let count = chunk.len().min(body_size as usize - buffer.len());
            match read_half.read(&mut chunk[..count]).await {
                Ok(0) | Err(_) => {
                    self.do_close();
                    return false;
                }
                Ok(read) => {
                    if buffer.try_reserve(read).is_err() {
                        self.do_close();
                        return false;
                    }
                    buffer.extend_from_slice(&chunk[..read]);
                }
            }
        }
        let callback = self.on_data.borrow().clone();
        if let Some(on_data) = callback {
            on_data(buffer);
        }
        true
    }
}

impl Drop for TcpChannel {
    fn drop(&mut self) {
        trace!("~TcpChannel");
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn dropping_unpolled_io_tasks_releases_channel_state() {
        let runtime = tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .unwrap();
        let tasks = tokio::task::LocalSet::new();
        let channel = TcpChannel::new(Rc::new(RefCell::new(TcpConfig::new())));
        let _peer = runtime.block_on(tasks.run_until(async {
            let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
            let peer = TcpStream::connect(listener.local_addr().unwrap())
                .await
                .unwrap();
            let (stream, _) = listener.accept().await.unwrap();
            channel.do_open(stream);
            peer
        }));
        assert_eq!(channel.active_loops.get(), 2);
        drop(tasks);
        assert_eq!(channel.active_loops.get(), 0);
        assert!(!channel.is_open());
        assert!(!channel.send(vec![1]));
    }

    #[test]
    fn callback_panic_finishes_io_and_allows_reopen() {
        for auto_pack in [false, true] {
            for panic_on_close in [false, true] {
                let runtime = tokio::runtime::Builder::new_current_thread()
                    .enable_all()
                    .build()
                    .unwrap();
                runtime.block_on(tokio::task::LocalSet::new().run_until(async {
                    tokio::time::timeout(std::time::Duration::from_secs(3), async {
                        let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
                        let mut peer = TcpStream::connect(listener.local_addr().unwrap())
                            .await
                            .unwrap();
                        let (stream, _) = listener.accept().await.unwrap();
                        let channel = TcpChannel::new(Rc::new(RefCell::new(TcpConfig {
                            auto_pack,
                            ..TcpConfig::new()
                        })));
                        let panics = Rc::new(Cell::new(0));
                        let count = panics.clone();
                        channel.on_data(move |_| {
                            count.set(count.get() + 1);
                            panic!("receive callback failed");
                        });
                        let closes = Rc::new(Cell::new(0));
                        let count = closes.clone();
                        let panic_count = panics.clone();
                        channel.on_close(move || {
                            count.set(count.get() + 1);
                            if panic_on_close {
                                panic_count.set(panic_count.get() + 1);
                                panic!("close callback failed");
                            }
                        });
                        channel.do_open(stream);
                        if panic_on_close {
                            channel.close();
                        } else {
                            if auto_pack {
                                peer.write_all(&1u32.to_le_bytes()).await.unwrap();
                            }
                            peer.write_all(b"x").await.unwrap();
                        }
                        channel.wait_close_finish().await;
                        assert!(!channel.is_open());
                        assert_eq!(panics.get(), 1);
                        assert_eq!(closes.get(), 1);
                        assert_eq!(peer.read(&mut [0]).await.unwrap(), 0);

                        let (tx, mut received) = tokio::sync::mpsc::unbounded_channel();
                        channel.on_data(move |data| {
                            tx.send(data).unwrap();
                        });
                        channel.on_close(|| {});
                        let mut peer = TcpStream::connect(listener.local_addr().unwrap())
                            .await
                            .unwrap();
                        let (stream, _) = listener.accept().await.unwrap();
                        channel.do_open(stream);
                        if auto_pack {
                            peer.write_all(&1u32.to_le_bytes()).await.unwrap();
                        }
                        peer.write_all(b"y").await.unwrap();
                        assert_eq!(received.recv().await.unwrap(), b"y");
                        channel.close();
                        channel.wait_close_finish().await;
                    })
                    .await
                    .expect("callback panic stranded IO shutdown");
                }));
            }
        }
    }

    #[test]
    fn default_frame_limit_is_shared_by_all_config_constructors() {
        use crate::net::config::DEFAULT_MAX_BODY_SIZE;
        use crate::net::config_builder::{RpcConfigBuilder, TcpConfigBuilder};
        assert_ne!(DEFAULT_MAX_BODY_SIZE, 0);
        assert_eq!(TcpConfig::new().max_body_size, DEFAULT_MAX_BODY_SIZE);
        assert_eq!(
            TcpConfigBuilder::new().build().max_body_size,
            DEFAULT_MAX_BODY_SIZE
        );
        assert_eq!(
            RpcConfigBuilder::new()
                .build()
                .to_tcp_config()
                .max_body_size,
            DEFAULT_MAX_BODY_SIZE
        );
        assert_eq!(
            RpcConfigBuilder::new()
                .max_body_size(0)
                .build()
                .max_body_size,
            0
        );
    }

    #[test]
    fn frame_body_reads_are_bounded_even_with_unlimited_frames() {
        struct HeaderOnly;
        impl AsyncRead for HeaderOnly {
            fn poll_read(
                self: std::pin::Pin<&mut Self>,
                _: &mut std::task::Context<'_>,
                buffer: &mut tokio::io::ReadBuf<'_>,
            ) -> std::task::Poll<std::io::Result<()>> {
                assert!(buffer.remaining() <= 8192);
                std::task::Poll::Ready(Ok(()))
            }
        }
        let runtime = tokio::runtime::Builder::new_current_thread()
            .build()
            .unwrap();
        runtime.block_on(async {
            let channel = TcpChannel::new(Rc::new(RefCell::new(TcpConfig {
                auto_pack: true,
                max_body_size: 0,
                ..TcpConfig::new()
            })));
            channel.on_data(|_| panic!("incomplete frame delivered"));
            *channel.is_open.borrow_mut() = true;
            assert!(
                !channel
                    .do_read_body(&mut HeaderOnly, 16 * 1024 * 1024)
                    .await
            );
            assert!(!channel.is_open());
        });
    }

    #[test]
    fn chunked_body_read_preserves_frame_boundaries_and_rejects_truncation() {
        let runtime = tokio::runtime::Builder::new_current_thread()
            .build()
            .unwrap();
        runtime.block_on(async {
            let channel = TcpChannel::new(Rc::new(RefCell::new(TcpConfig::new())));
            let received = Rc::new(RefCell::new(Vec::new()));
            let output = received.clone();
            channel.on_data(move |body| output.borrow_mut().push(body));
            let body = vec![42; 20000];
            let input = [body.clone(), b"next".to_vec()].concat();
            let mut reader = input.as_slice();
            assert!(channel.do_read_body(&mut reader, body.len() as u32).await);
            assert_eq!(reader, b"next");
            assert_eq!(received.borrow()[0], body);
            assert!(channel.do_read_body(&mut reader, 0).await);
            assert_eq!(reader, b"next");
            assert!(received.borrow()[1].is_empty());
            *channel.is_open.borrow_mut() = true;
            assert!(!channel.do_read_body(&mut reader, 5).await);
            assert!(!channel.is_open());
            assert_eq!(received.borrow().len(), 2);
        });
    }

    #[test]
    fn all_close_waiters_observe_finished_io() {
        let runtime = tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .unwrap();
        runtime.block_on(tokio::task::LocalSet::new().run_until(async {
            tokio::time::timeout(std::time::Duration::from_secs(1), async {
                let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
                let _peer = TcpStream::connect(listener.local_addr().unwrap())
                    .await
                    .unwrap();
                let (stream, _) = listener.accept().await.unwrap();
                let channel = TcpChannel::new(Rc::new(RefCell::new(TcpConfig::new())));
                channel.do_open(stream);
                channel.close();
                tokio::join!(channel.wait_close_finish(), channel.wait_close_finish());
                channel.wait_close_finish().await;
            })
            .await
            .unwrap();
        }));
    }

    #[test]
    fn reopening_does_not_send_old_frames_or_orphaned_bodies() {
        let runtime = tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .unwrap();
        runtime.block_on(tokio::task::LocalSet::new().run_until(async {
            tokio::time::timeout(std::time::Duration::from_secs(3), async {
                let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
                let channel = TcpChannel::new(Rc::new(RefCell::new(TcpConfig {
                    auto_pack: true,
                    ..TcpConfig::new()
                })));
                for orphaned_body in [false, true] {
                    let _old_peer = TcpStream::connect(listener.local_addr().unwrap())
                        .await
                        .unwrap();
                    let (old_stream, _) = listener.accept().await.unwrap();
                    channel.do_open(old_stream);
                    channel.send_str("old");
                    if orphaned_body {
                        channel.send_queue.borrow_mut().pop_front();
                    }
                    channel.close();
                    channel.wait_close_finish().await;
                    assert!(channel.send_queue.borrow().is_empty());
                    let mut peer = TcpStream::connect(listener.local_addr().unwrap())
                        .await
                        .unwrap();
                    let (stream, _) = listener.accept().await.unwrap();
                    channel.do_open(stream);
                    channel.send_str("new");
                    assert_eq!(peer.read_u32_le().await.unwrap(), 3);
                    let mut body = [0; 3];
                    peer.read_exact(&mut body).await.unwrap();
                    assert_eq!(&body, b"new");
                    channel.close();
                    channel.wait_close_finish().await;
                }
            })
            .await
            .unwrap();
        }));
    }

    #[test]
    fn raw_channel_can_close_before_read_task_starts() {
        let runtime = tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .unwrap();
        runtime.block_on(tokio::task::LocalSet::new().run_until(async {
            tokio::time::timeout(std::time::Duration::from_secs(3), async {
                let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
                let _peer = TcpStream::connect(listener.local_addr().unwrap())
                    .await
                    .unwrap();
                let (stream, _) = listener.accept().await.unwrap();
                let channel = TcpChannel::new(Rc::new(RefCell::new(TcpConfig::new())));
                channel.do_open(stream);
                channel.close();
                channel.wait_close_finish().await;
            })
            .await
            .expect("read task missed early close");
        }));
    }

    #[test]
    fn oversized_header_closes_before_reading_body() {
        let runtime = tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .unwrap();
        runtime.block_on(tokio::task::LocalSet::new().run_until(async {
            tokio::time::timeout(std::time::Duration::from_secs(3), async {
                let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
                let mut peer = TcpStream::connect(listener.local_addr().unwrap())
                    .await
                    .unwrap();
                let (stream, _) = listener.accept().await.unwrap();
                let channel = TcpChannel::new(Rc::new(RefCell::new(TcpConfig {
                    auto_pack: true,
                    max_body_size: 8,
                    ..TcpConfig::new()
                })));
                channel.on_data(|_| panic!("oversized packet delivered"));
                channel.do_open(stream);
                peer.write_all(&9u32.to_le_bytes()).await.unwrap();
                channel.wait_close_finish().await;
                assert!(!channel.is_open());
            })
            .await
            .expect("oversized header was not rejected");
        }));
    }

    #[test]
    fn send_limit_rejects_whole_frames_and_counts_headers() {
        for auto_pack in [false, true] {
            let config = Rc::new(RefCell::new(TcpConfig {
                auto_pack,
                max_send_buffer_size: 8,
                ..TcpConfig::new()
            }));
            let channel = TcpChannel::new(config);
            *channel.is_open.borrow_mut() = true;
            let body = vec![1; if auto_pack { 4 } else { 8 }];
            assert!(!channel.send(vec![0; 9]));
            assert!(channel.send_queue.borrow().is_empty());
            assert!(channel.send(body.clone()));
            assert!(!channel.send(vec![2]));
            if auto_pack {
                assert!(!channel.send(Vec::new()));
            }
            let queued: Vec<u8> = channel
                .send_queue
                .borrow()
                .iter()
                .flatten()
                .copied()
                .collect();
            let expected = if auto_pack {
                [4u32.to_le_bytes().to_vec(), body].concat()
            } else {
                body
            };
            assert_eq!(queued, expected);
            channel.close();
            assert_eq!(channel.send_buffer_size.get(), 0);
            *channel.is_open.borrow_mut() = true;
            assert!(channel.send(vec![3]));
        }
    }

    #[test]
    fn completed_writes_restore_send_capacity() {
        let runtime = tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .unwrap();
        runtime.block_on(tokio::task::LocalSet::new().run_until(async {
            tokio::time::timeout(std::time::Duration::from_secs(3), async {
                let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
                let mut peer = TcpStream::connect(listener.local_addr().unwrap())
                    .await
                    .unwrap();
                let (stream, _) = listener.accept().await.unwrap();
                let channel = TcpChannel::new(Rc::new(RefCell::new(TcpConfig {
                    auto_pack: true,
                    max_send_buffer_size: 8,
                    ..TcpConfig::new()
                })));
                channel.do_open(stream);
                for _ in 0..2 {
                    assert!(channel.send(b"data".to_vec()));
                    assert!(!channel.send(b"extra".to_vec()));
                    assert_eq!(peer.read_u32_le().await.unwrap(), 4);
                    let mut body = [0; 4];
                    peer.read_exact(&mut body).await.unwrap();
                    assert_eq!(&body, b"data");
                    assert_eq!(channel.send_buffer_size.get(), 0);
                }
                channel.close();
                channel.wait_close_finish().await;
            })
            .await
            .unwrap();
        }));
    }

    #[test]
    fn empty_packed_frame_queues_a_header() {
        let config = Rc::new(RefCell::new(TcpConfig {
            auto_pack: true,
            ..TcpConfig::new()
        }));
        let channel = TcpChannel::new(config);
        *channel.is_open.borrow_mut() = true;

        channel.send(Vec::new());
        channel.send(b"next".to_vec());

        let queue = channel.send_queue.borrow();
        assert_eq!(queue.len(), 3);
        assert_eq!(queue[0], 0u32.to_le_bytes());
        assert_eq!(queue[1], 4u32.to_le_bytes());
        assert_eq!(queue[2], b"next");
    }
}
