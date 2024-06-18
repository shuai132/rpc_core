#pragma once

// clang-format off
#define RPC_CORE_DETAIL_SERIALIZE_GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, _62, _63, _64, NAME,...) NAME
#define RPC_CORE_DETAIL_SERIALIZE_PASTE(...) RPC_CORE_DETAIL_SERIALIZE_GET_MACRO(__VA_ARGS__, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE64, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE63, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE62, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE61, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE60, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE59, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE58, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE57, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE56, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE55, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE54, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE53, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE52, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE51, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE50, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE49, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE48, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE47, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE46, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE45, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE44, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE43, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE42, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE41, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE40, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE39, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE38, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE37, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE36, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE35, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE34, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE33, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE32, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE31, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE30, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE29, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE28, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE27, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE26, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE25, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE24, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE23, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE22, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE21, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE20, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE19, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE18, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE17, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE16, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE15, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE14, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE13, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE12, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE11, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE10, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE9, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE8, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE7, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE6, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE5, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE4, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE3, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE2, \
        RPC_CORE_DETAIL_SERIALIZE_PASTE1)(__VA_ARGS__)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) func(v1)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE3(func, v1, v2) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v2)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE4(func, v1, v2, v3) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE3(func, v2, v3)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE5(func, v1, v2, v3, v4) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE4(func, v2, v3, v4)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE6(func, v1, v2, v3, v4, v5) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE5(func, v2, v3, v4, v5)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE7(func, v1, v2, v3, v4, v5, v6) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE6(func, v2, v3, v4, v5, v6)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE8(func, v1, v2, v3, v4, v5, v6, v7) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE7(func, v2, v3, v4, v5, v6, v7)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE9(func, v1, v2, v3, v4, v5, v6, v7, v8) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE8(func, v2, v3, v4, v5, v6, v7, v8)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE10(func, v1, v2, v3, v4, v5, v6, v7, v8, v9) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE9(func, v2, v3, v4, v5, v6, v7, v8, v9)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE11(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE10(func, v2, v3, v4, v5, v6, v7, v8, v9, v10)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE12(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE11(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE13(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE12(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE14(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE13(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE15(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE14(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE16(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE15(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE17(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE16(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE18(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE17(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE19(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE18(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE20(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE19(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE21(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE20(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE22(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE21(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE23(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE22(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE24(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE23(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE25(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE24(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE26(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE25(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE27(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE26(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE28(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE27(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE29(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE28(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE30(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE29(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE31(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE30(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE32(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE31(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE33(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE32(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE34(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE33(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE35(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE34(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE36(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE35(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE37(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE36(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE38(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE37(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE39(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE38(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE40(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE39(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE41(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE40(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE42(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE41(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE43(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE42(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE44(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE43(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE45(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE44(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE46(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE45(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE47(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE46(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE48(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE47(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE49(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE48(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE50(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE49(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE51(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE50(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE52(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE51(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE53(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE52(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE54(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE53(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE55(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE54(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE56(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE55(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE57(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE56(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE58(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE57(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE59(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE58(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE60(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE59(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE61(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE60(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE62(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE61(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE63(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE62(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62)
#define RPC_CORE_DETAIL_SERIALIZE_PASTE64(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63) RPC_CORE_DETAIL_SERIALIZE_PASTE2(func, v1) RPC_CORE_DETAIL_SERIALIZE_PASTE63(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63)

#define RPC_CORE_DETAIL_SERIALIZE_FIELD(v1) ar & t.v1;
#define RPC_CORE_DETAIL_SERIALIZE_FIELD_INNER(v1) ar & this->v1;
// clang-format on

#include <string>
#include <type_traits>

#include "../detail/noncopyable.hpp"
#include "../detail/string_view.hpp"

namespace rpc_core {

template <typename T, typename std::enable_if<std::is_fundamental<T>::value, int>::type = 0>
inline serialize_oarchive& operator&(serialize_oarchive& oa, const T& t) {
  t >> oa;
  return oa;
}

template <typename T, typename std::enable_if<!std::is_fundamental<T>::value, int>::type = 0>
inline serialize_oarchive& operator&(serialize_oarchive& oa, const T& t) {
  serialize_oarchive tmp;
  t >> tmp;
  tmp >> oa;
  return oa;
}

template <typename T, typename std::enable_if<std::is_fundamental<T>::value, int>::type = 0>
inline serialize_iarchive& operator&(serialize_iarchive& ia, T& t) {
  if (ia.error) return ia;
  t << ia;
  return ia;
}

template <typename T, typename std::enable_if<!std::is_fundamental<T>::value, int>::type = 0>
serialize_iarchive& operator&(serialize_iarchive& ia, T& t) {
  if (ia.error) return ia;
  detail::auto_size auto_size;
  int cost = auto_size.deserialize(ia.data);
  auto size = auto_size.value;
  ia.data += cost;

  serialize_iarchive tmp(detail::string_view(ia.data, size));
  t << tmp;
  ia.error = tmp.error;

  ia.data += size;
  return ia;
}

}  // namespace rpc_core

#define RPC_CORE_DEFINE_TYPE(Type, ...)                                           \
  inline void operator>>(const Type& t, ::rpc_core::serialize_oarchive& ar) {     \
    RPC_CORE_DETAIL_SERIALIZE_PASTE(RPC_CORE_DETAIL_SERIALIZE_FIELD, __VA_ARGS__) \
  }                                                                               \
  inline void operator<<(Type& t, ::rpc_core::serialize_iarchive& ar) {           \
    RPC_CORE_DETAIL_SERIALIZE_PASTE(RPC_CORE_DETAIL_SERIALIZE_FIELD, __VA_ARGS__) \
  }

#define RPC_CORE_DEFINE_TYPE_INNER(...)                                                 \
 public:                                                                                \
  void operator>>(::rpc_core::serialize_oarchive& ar) const {                           \
    RPC_CORE_DETAIL_SERIALIZE_PASTE(RPC_CORE_DETAIL_SERIALIZE_FIELD_INNER, __VA_ARGS__) \
  }                                                                                     \
  void operator<<(::rpc_core::serialize_iarchive& ar) {                                 \
    RPC_CORE_DETAIL_SERIALIZE_PASTE(RPC_CORE_DETAIL_SERIALIZE_FIELD_INNER, __VA_ARGS__) \
  }
