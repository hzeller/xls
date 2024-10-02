// RUN: xls/contrib/mlir/xls_opt -elaborate-procs %s 2>&1 | FileCheck %s
// CHECK:       xls.chan @req : i32
// CHECK-NEXT:  xls.chan @resp : i32
// CHECK-NEXT:  xls.chan @rom1_req : i32
// CHECK-NEXT:  xls.chan @rom1_resp : i32
// CHECK-NEXT:  xls.eproc @rom_0(%arg0: i32) zeroinitializer {
// CHECK-NEXT:    %0 = xls.after_all  : !xls.token
// CHECK-NEXT:    %tkn_out, %result = xls.blocking_receive %0, @rom1_req : i32
// CHECK-NEXT:    %1 = "xls.constant_scalar"() <{value = 1 : i32}> : () -> i32
// CHECK-NEXT:    %2 = xls.send %tkn_out, %1, @rom1_resp : i32
// CHECK-NEXT:    xls.yield %arg0 : i32
// CHECK-NEXT:  }
// CHECK-NEXT:  xls.chan @rom2_req : i32
// CHECK-NEXT:  xls.chan @rom2_resp : i32
// CHECK-NEXT:  xls.eproc @rom_1(%arg0: i32) zeroinitializer {
// CHECK-NEXT:    %0 = xls.after_all  : !xls.token
// CHECK-NEXT:    %tkn_out, %result = xls.blocking_receive %0, @rom2_req : i32
// CHECK-NEXT:    %1 = "xls.constant_scalar"() <{value = 1 : i32}> : () -> i32
// CHECK-NEXT:    %2 = xls.send %tkn_out, %1, @rom2_resp : i32
// CHECK-NEXT:    xls.yield %arg0 : i32
// CHECK-NEXT:  }
// CHECK-NEXT:  xls.eproc @proxy_0(%arg0: i32) zeroinitializer {
// CHECK-NEXT:    %0 = xls.after_all  : !xls.token
// CHECK-NEXT:    %tkn_out, %result = xls.blocking_receive %0, @req : i32
// CHECK-NEXT:    %1 = xls.send %tkn_out, %result, @rom1_req : i32
// CHECK-NEXT:    %tkn_out_0, %result_1 = xls.blocking_receive %1, @rom1_resp : i32
// CHECK-NEXT:    %2 = xls.send %tkn_out_0, %result_1, @resp : i32
// CHECK-NEXT:    xls.yield %arg0 : i32
// CHECK-NEXT:  }
// CHECK-NEXT:  xls.eproc @fetch_0(%arg0: i32) zeroinitializer {
// CHECK-NEXT:    %0 = xls.after_all  : !xls.token
// CHECK-NEXT:    %1 = xls.send %0, %arg0, @req : i32
// CHECK-NEXT:    %tkn_out, %result = xls.blocking_receive %1, @resp : i32
// CHECK-NEXT:    xls.yield %result : i32
// CHECK-NEXT:  }
// CHECK-NEXT:  xls.chan @boundary1 {send_supported = false} : i32
// CHECK-NEXT:  xls.chan @boundary2 {recv_supported = false} : i32
// CHECK-NEXT:  xls.eproc @rom_2(%arg0: i32) zeroinitializer {
// CHECK-NEXT:    %0 = xls.after_all  : !xls.token
// CHECK-NEXT:    %tkn_out, %result = xls.blocking_receive %0, @boundary1 : i32
// CHECK-NEXT:    %1 = "xls.constant_scalar"() <{value = 1 : i32}> : () -> i32
// CHECK-NEXT:    %2 = xls.send %tkn_out, %1, @boundary2 : i32
// CHECK-NEXT:    xls.yield %arg0 : i32
// CHECK-NEXT:  }

xls.sproc @fetch() top {
  spawns {
    %req_out, %req_in = xls.schan<i32>("req")
    %resp_out, %resp_in = xls.schan<i32>("resp")
    xls.spawn @proxy(%req_in, %resp_out) : !xls.schan<i32, in>, !xls.schan<i32, out>
    xls.yield %req_out, %resp_in : !xls.schan<i32, out>, !xls.schan<i32, in>
  }
  next (%req: !xls.schan<i32, out>, %resp: !xls.schan<i32, in>, %state: i32) zeroinitializer {
    %tok1 = xls.after_all : !xls.token
    %tok2 = xls.ssend %tok1, %state, %req : (!xls.token, i32, !xls.schan<i32, out>) -> !xls.token
    %tok3, %result = xls.sblocking_receive %tok2, %resp : (!xls.token, !xls.schan<i32, in>) -> (!xls.token, i32)
    xls.yield %result : i32
  }
}

xls.sproc @proxy(%req: !xls.schan<i32, in>, %resp: !xls.schan<i32, out>) {
  spawns {
    %rom1_req_out, %rom1_req_in = xls.schan<i32>("rom1_req")
    %rom1_resp_out, %rom1_resp_in = xls.schan<i32>("rom1_resp")
    xls.spawn @rom(%rom1_req_in, %rom1_resp_out) : !xls.schan<i32, in>, !xls.schan<i32, out>
    %rom2_req_out, %rom2_req_in = xls.schan<i32>("rom2_req")
    %rom2_resp_out, %rom2_resp_in = xls.schan<i32>("rom2_resp")
    xls.spawn @rom(%rom2_req_in, %rom2_resp_out) : !xls.schan<i32, in>, !xls.schan<i32, out>
    xls.yield %req, %resp, %rom1_req_out, %rom1_resp_in : !xls.schan<i32, in>, !xls.schan<i32, out>, !xls.schan<i32, out>, !xls.schan<i32, in>
  }
  next (%req: !xls.schan<i32, in>, %resp: !xls.schan<i32, out>, %rom_req: !xls.schan<i32, out>, %rom_resp: !xls.schan<i32, in>, %state: i32) zeroinitializer {
    %tok1 = xls.after_all : !xls.token
    %tok2, %result = xls.sblocking_receive %tok1, %req : (!xls.token, !xls.schan<i32, in>) -> (!xls.token, i32)
    %tok3 = xls.ssend %tok2, %result, %rom_req : (!xls.token, i32, !xls.schan<i32, out>) -> !xls.token
    %tok4, %result2 = xls.sblocking_receive %tok3, %rom_resp : (!xls.token, !xls.schan<i32, in>) -> (!xls.token, i32)
    %tok5 = xls.ssend %tok4, %result2, %resp : (!xls.token, i32, !xls.schan<i32, out>) -> !xls.token
    xls.yield %state : i32
  }
}

xls.sproc @rom(%req: !xls.schan<i32, in>, %resp: !xls.schan<i32, out>) top attributes {boundary_channel_names = ["boundary1", "boundary2"]} {
  spawns {
    xls.yield %req, %resp : !xls.schan<i32, in>, !xls.schan<i32, out>
  }
  next (%req: !xls.schan<i32, in>, %resp: !xls.schan<i32, out>, %state: i32) zeroinitializer {
    %tok0 = xls.after_all : !xls.token
    %tok1, %address = xls.sblocking_receive %tok0, %req : (!xls.token, !xls.schan<i32, in>) -> (!xls.token, i32)
    %one = "xls.constant_scalar"() {value = 1 : i32} : () -> i32
    %tok2 = xls.ssend %tok1, %one, %resp : (!xls.token, i32, !xls.schan<i32, out>) -> !xls.token
    xls.yield %state : i32
  }
}