use std::rc::Rc;

struct SomeStruct {
    // 结构体的定义
}

impl SomeStruct {
    fn some_method(self: Rc<Self>) {}
}

fn main() {
    let some_struct = SomeStruct {};
    let arc_some_struct = Rc::new(some_struct);

    arc_some_struct.some_method();
}
