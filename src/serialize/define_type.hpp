#pragma once

// clang-format off
#define RpcCore_DETAIL_SERIALIZE_GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, _62, _63, _64, NAME,...) NAME
#define RpcCore_DETAIL_SERIALIZE_PASTE(...) RpcCore_DETAIL_SERIALIZE_GET_MACRO(__VA_ARGS__, \
        RpcCore_DETAIL_SERIALIZE_PASTE64, \
        RpcCore_DETAIL_SERIALIZE_PASTE63, \
        RpcCore_DETAIL_SERIALIZE_PASTE62, \
        RpcCore_DETAIL_SERIALIZE_PASTE61, \
        RpcCore_DETAIL_SERIALIZE_PASTE60, \
        RpcCore_DETAIL_SERIALIZE_PASTE59, \
        RpcCore_DETAIL_SERIALIZE_PASTE58, \
        RpcCore_DETAIL_SERIALIZE_PASTE57, \
        RpcCore_DETAIL_SERIALIZE_PASTE56, \
        RpcCore_DETAIL_SERIALIZE_PASTE55, \
        RpcCore_DETAIL_SERIALIZE_PASTE54, \
        RpcCore_DETAIL_SERIALIZE_PASTE53, \
        RpcCore_DETAIL_SERIALIZE_PASTE52, \
        RpcCore_DETAIL_SERIALIZE_PASTE51, \
        RpcCore_DETAIL_SERIALIZE_PASTE50, \
        RpcCore_DETAIL_SERIALIZE_PASTE49, \
        RpcCore_DETAIL_SERIALIZE_PASTE48, \
        RpcCore_DETAIL_SERIALIZE_PASTE47, \
        RpcCore_DETAIL_SERIALIZE_PASTE46, \
        RpcCore_DETAIL_SERIALIZE_PASTE45, \
        RpcCore_DETAIL_SERIALIZE_PASTE44, \
        RpcCore_DETAIL_SERIALIZE_PASTE43, \
        RpcCore_DETAIL_SERIALIZE_PASTE42, \
        RpcCore_DETAIL_SERIALIZE_PASTE41, \
        RpcCore_DETAIL_SERIALIZE_PASTE40, \
        RpcCore_DETAIL_SERIALIZE_PASTE39, \
        RpcCore_DETAIL_SERIALIZE_PASTE38, \
        RpcCore_DETAIL_SERIALIZE_PASTE37, \
        RpcCore_DETAIL_SERIALIZE_PASTE36, \
        RpcCore_DETAIL_SERIALIZE_PASTE35, \
        RpcCore_DETAIL_SERIALIZE_PASTE34, \
        RpcCore_DETAIL_SERIALIZE_PASTE33, \
        RpcCore_DETAIL_SERIALIZE_PASTE32, \
        RpcCore_DETAIL_SERIALIZE_PASTE31, \
        RpcCore_DETAIL_SERIALIZE_PASTE30, \
        RpcCore_DETAIL_SERIALIZE_PASTE29, \
        RpcCore_DETAIL_SERIALIZE_PASTE28, \
        RpcCore_DETAIL_SERIALIZE_PASTE27, \
        RpcCore_DETAIL_SERIALIZE_PASTE26, \
        RpcCore_DETAIL_SERIALIZE_PASTE25, \
        RpcCore_DETAIL_SERIALIZE_PASTE24, \
        RpcCore_DETAIL_SERIALIZE_PASTE23, \
        RpcCore_DETAIL_SERIALIZE_PASTE22, \
        RpcCore_DETAIL_SERIALIZE_PASTE21, \
        RpcCore_DETAIL_SERIALIZE_PASTE20, \
        RpcCore_DETAIL_SERIALIZE_PASTE19, \
        RpcCore_DETAIL_SERIALIZE_PASTE18, \
        RpcCore_DETAIL_SERIALIZE_PASTE17, \
        RpcCore_DETAIL_SERIALIZE_PASTE16, \
        RpcCore_DETAIL_SERIALIZE_PASTE15, \
        RpcCore_DETAIL_SERIALIZE_PASTE14, \
        RpcCore_DETAIL_SERIALIZE_PASTE13, \
        RpcCore_DETAIL_SERIALIZE_PASTE12, \
        RpcCore_DETAIL_SERIALIZE_PASTE11, \
        RpcCore_DETAIL_SERIALIZE_PASTE10, \
        RpcCore_DETAIL_SERIALIZE_PASTE9, \
        RpcCore_DETAIL_SERIALIZE_PASTE8, \
        RpcCore_DETAIL_SERIALIZE_PASTE7, \
        RpcCore_DETAIL_SERIALIZE_PASTE6, \
        RpcCore_DETAIL_SERIALIZE_PASTE5, \
        RpcCore_DETAIL_SERIALIZE_PASTE4, \
        RpcCore_DETAIL_SERIALIZE_PASTE3, \
        RpcCore_DETAIL_SERIALIZE_PASTE2, \
        RpcCore_DETAIL_SERIALIZE_PASTE1)(__VA_ARGS__)
#define RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) func(v1)
#define RpcCore_DETAIL_SERIALIZE_PASTE3(func, v1, v2) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v2)
#define RpcCore_DETAIL_SERIALIZE_PASTE4(func, v1, v2, v3) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE3(func, v2, v3)
#define RpcCore_DETAIL_SERIALIZE_PASTE5(func, v1, v2, v3, v4) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE4(func, v2, v3, v4)
#define RpcCore_DETAIL_SERIALIZE_PASTE6(func, v1, v2, v3, v4, v5) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE5(func, v2, v3, v4, v5)
#define RpcCore_DETAIL_SERIALIZE_PASTE7(func, v1, v2, v3, v4, v5, v6) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE6(func, v2, v3, v4, v5, v6)
#define RpcCore_DETAIL_SERIALIZE_PASTE8(func, v1, v2, v3, v4, v5, v6, v7) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE7(func, v2, v3, v4, v5, v6, v7)
#define RpcCore_DETAIL_SERIALIZE_PASTE9(func, v1, v2, v3, v4, v5, v6, v7, v8) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE8(func, v2, v3, v4, v5, v6, v7, v8)
#define RpcCore_DETAIL_SERIALIZE_PASTE10(func, v1, v2, v3, v4, v5, v6, v7, v8, v9) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE9(func, v2, v3, v4, v5, v6, v7, v8, v9)
#define RpcCore_DETAIL_SERIALIZE_PASTE11(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE10(func, v2, v3, v4, v5, v6, v7, v8, v9, v10)
#define RpcCore_DETAIL_SERIALIZE_PASTE12(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE11(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11)
#define RpcCore_DETAIL_SERIALIZE_PASTE13(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE12(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12)
#define RpcCore_DETAIL_SERIALIZE_PASTE14(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE13(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13)
#define RpcCore_DETAIL_SERIALIZE_PASTE15(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE14(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14)
#define RpcCore_DETAIL_SERIALIZE_PASTE16(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE15(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15)
#define RpcCore_DETAIL_SERIALIZE_PASTE17(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE16(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16)
#define RpcCore_DETAIL_SERIALIZE_PASTE18(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE17(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17)
#define RpcCore_DETAIL_SERIALIZE_PASTE19(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE18(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18)
#define RpcCore_DETAIL_SERIALIZE_PASTE20(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE19(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19)
#define RpcCore_DETAIL_SERIALIZE_PASTE21(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE20(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20)
#define RpcCore_DETAIL_SERIALIZE_PASTE22(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE21(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21)
#define RpcCore_DETAIL_SERIALIZE_PASTE23(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE22(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22)
#define RpcCore_DETAIL_SERIALIZE_PASTE24(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE23(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23)
#define RpcCore_DETAIL_SERIALIZE_PASTE25(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE24(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24)
#define RpcCore_DETAIL_SERIALIZE_PASTE26(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE25(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25)
#define RpcCore_DETAIL_SERIALIZE_PASTE27(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE26(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26)
#define RpcCore_DETAIL_SERIALIZE_PASTE28(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE27(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27)
#define RpcCore_DETAIL_SERIALIZE_PASTE29(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE28(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28)
#define RpcCore_DETAIL_SERIALIZE_PASTE30(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE29(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29)
#define RpcCore_DETAIL_SERIALIZE_PASTE31(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE30(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30)
#define RpcCore_DETAIL_SERIALIZE_PASTE32(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE31(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31)
#define RpcCore_DETAIL_SERIALIZE_PASTE33(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE32(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32)
#define RpcCore_DETAIL_SERIALIZE_PASTE34(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE33(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33)
#define RpcCore_DETAIL_SERIALIZE_PASTE35(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE34(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34)
#define RpcCore_DETAIL_SERIALIZE_PASTE36(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE35(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35)
#define RpcCore_DETAIL_SERIALIZE_PASTE37(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE36(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36)
#define RpcCore_DETAIL_SERIALIZE_PASTE38(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE37(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37)
#define RpcCore_DETAIL_SERIALIZE_PASTE39(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE38(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38)
#define RpcCore_DETAIL_SERIALIZE_PASTE40(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE39(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39)
#define RpcCore_DETAIL_SERIALIZE_PASTE41(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE40(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40)
#define RpcCore_DETAIL_SERIALIZE_PASTE42(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE41(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41)
#define RpcCore_DETAIL_SERIALIZE_PASTE43(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE42(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42)
#define RpcCore_DETAIL_SERIALIZE_PASTE44(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE43(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43)
#define RpcCore_DETAIL_SERIALIZE_PASTE45(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE44(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44)
#define RpcCore_DETAIL_SERIALIZE_PASTE46(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE45(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45)
#define RpcCore_DETAIL_SERIALIZE_PASTE47(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE46(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46)
#define RpcCore_DETAIL_SERIALIZE_PASTE48(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE47(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47)
#define RpcCore_DETAIL_SERIALIZE_PASTE49(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE48(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48)
#define RpcCore_DETAIL_SERIALIZE_PASTE50(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE49(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49)
#define RpcCore_DETAIL_SERIALIZE_PASTE51(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE50(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50)
#define RpcCore_DETAIL_SERIALIZE_PASTE52(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE51(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51)
#define RpcCore_DETAIL_SERIALIZE_PASTE53(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE52(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52)
#define RpcCore_DETAIL_SERIALIZE_PASTE54(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE53(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53)
#define RpcCore_DETAIL_SERIALIZE_PASTE55(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE54(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54)
#define RpcCore_DETAIL_SERIALIZE_PASTE56(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE55(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55)
#define RpcCore_DETAIL_SERIALIZE_PASTE57(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE56(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56)
#define RpcCore_DETAIL_SERIALIZE_PASTE58(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE57(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57)
#define RpcCore_DETAIL_SERIALIZE_PASTE59(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE58(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58)
#define RpcCore_DETAIL_SERIALIZE_PASTE60(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE59(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59)
#define RpcCore_DETAIL_SERIALIZE_PASTE61(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE60(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60)
#define RpcCore_DETAIL_SERIALIZE_PASTE62(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE61(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61)
#define RpcCore_DETAIL_SERIALIZE_PASTE63(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE62(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62)
#define RpcCore_DETAIL_SERIALIZE_PASTE64(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63) RpcCore_DETAIL_SERIALIZE_PASTE2(func, v1) RpcCore_DETAIL_SERIALIZE_PASTE63(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63)

#define RpcCore_DETAIL_SERIALIZE_FIELD(v1) ar & t.v1;
// clang-format on

#include <string>
#include <type_traits>

#include "../Serialize.hpp"
#include "../detail/noncopyable.hpp"
#include "../detail/string_view.hpp"

namespace RpcCore {

struct serialize_oarchive : detail::noncopyable {
  std::string ss_;
  bool inner_ = false;
};

struct serialize_iarchive : detail::noncopyable {
  serialize_iarchive(detail::string_view sv) : data_((char*)sv.data()), size_(sv.size()) {}  // NOLINT(google-explicit-constructor)
  char* data_;
  size_t size_;
  bool error_ = false;
};

}  // namespace RpcCore

namespace RpcCore {

template <typename T, typename std::enable_if<std::is_fundamental<T>::value, int>::type = 0>
serialize_oarchive& operator&(serialize_oarchive& oa, const T& t);

template <typename T, typename std::enable_if<!std::is_fundamental<T>::value, int>::type = 0>
serialize_oarchive& operator&(serialize_oarchive& oa, const T& t);

template <class T, typename std::enable_if<std::is_fundamental<T>::value, int>::type = 0>
serialize_iarchive& operator&(serialize_iarchive& ia, T& t);

template <class T, typename std::enable_if<!std::is_fundamental<T>::value, int>::type = 0>
serialize_iarchive& operator&(serialize_iarchive& ia, T& t);

}  // namespace RpcCore

#define RpcCore_DEFINE_TYPE(Type, ...)                                                         \
  namespace RpcCore {                                                                          \
  template <typename T, typename std::enable_if<std::is_same<T, Type>::value, int>::type = 0>  \
  inline std::string serialize(const T& t) {                                                   \
    serialize_oarchive ar;                                                                     \
    RpcCore_DETAIL_SERIALIZE_PASTE(RpcCore_DETAIL_SERIALIZE_FIELD, __VA_ARGS__) return ar.ss_; \
  }                                                                                            \
  template <typename T, typename std::enable_if<std::is_same<T, Type>::value, int>::type = 0>  \
  inline bool deserialize(const detail::string_view& data, T& t) {                             \
    serialize_iarchive ar(data);                                                               \
    RpcCore_DETAIL_SERIALIZE_PASTE(RpcCore_DETAIL_SERIALIZE_FIELD, __VA_ARGS__) return true;   \
  }                                                                                            \
  }
