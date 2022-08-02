(module
  (type (;0;) (func (param i64) (result i64)))
  (type (;1;) (func (param i32 i32) (result i32)))
  (type (;2;) (func (param i32)))
  (type (;3;) (func))
  (import "env" "setTempRet0" (func (;0;) (type 2)))
  (func (;1;) (type 3)
    nop)
  (func (;2;) (type 0) (param i64) (result i64)
    (local i64 i64 i64 i64)
    block  ;; label = @1
      block  ;; label = @2
        local.get 0
        i64.const 3
        i64.ge_u
        if  ;; label = @3
          local.get 0
          i64.const 2
          i64.sub
          local.tee 1
          i64.const 7
          i64.and
          local.set 3
          local.get 0
          i64.const 3
          i64.sub
          i64.const 7
          i64.ge_u
          br_if 1 (;@2;)
          i64.const 1
          local.set 0
          i64.const 1
          local.set 1
          br 2 (;@1;)
        end
        local.get 0
        i64.const 0
        i64.ne
        i64.extend_i32_u
        return
      end
      local.get 1
      i64.const -8
      i64.and
      local.set 4
      i64.const 1
      local.set 0
      i64.const 1
      local.set 1
      loop  ;; label = @2
        local.get 0
        local.get 1
        i64.add
        local.tee 1
        local.get 0
        i64.add
        local.tee 0
        local.get 1
        i64.add
        local.tee 1
        local.get 0
        i64.add
        local.tee 0
        local.get 1
        i64.add
        local.tee 1
        local.get 0
        i64.add
        local.tee 0
        local.get 1
        i64.add
        local.tee 1
        local.get 0
        i64.add
        local.set 0
        local.get 2
        i64.const 8
        i64.add
        local.tee 2
        local.get 4
        i64.ne
        br_if 0 (;@2;)
      end
    end
    local.get 3
    i64.eqz
    i32.eqz
    if  ;; label = @1
      i64.const 0
      local.set 2
      loop  ;; label = @2
        local.get 1
        local.get 0
        local.tee 1
        i64.add
        local.set 0
        local.get 2
        i64.const 1
        i64.add
        local.tee 2
        local.get 3
        i64.ne
        br_if 0 (;@2;)
      end
    end
    local.get 0)
  (func (;3;) (type 0) (param i64) (result i64)
    (local i64)
    local.get 0
    i64.const 2
    i64.ge_u
    if  ;; label = @1
      loop  ;; label = @2
        local.get 0
        i64.const 1
        i64.sub
        call 3
        local.get 1
        i64.add
        local.set 1
        local.get 0
        i64.const 2
        i64.sub
        local.tee 0
        i64.const 1
        i64.gt_u
        br_if 0 (;@2;)
      end
    end
    local.get 0
    local.get 1
    i64.add)
  (func (;4;) (type 1) (param i32 i32) (result i32)
    (local i64)
    local.get 0
    i64.extend_i32_u
    local.get 1
    i64.extend_i32_u
    i64.const 32
    i64.shl
    i64.or
    call 2
    local.tee 2
    i64.const 32
    i64.shr_u
    i32.wrap_i64
    call 0
    local.get 2
    i32.wrap_i64)
  (func (;5;) (type 1) (param i32 i32) (result i32)
    (local i64)
    local.get 0
    i64.extend_i32_u
    local.get 1
    i64.extend_i32_u
    i64.const 32
    i64.shl
    i64.or
    call 3
    local.tee 2
    i64.const 32
    i64.shr_u
    i32.wrap_i64
    call 0
    local.get 2
    i32.wrap_i64)
  (export "__wasm_call_ctors" (func 1))
  (export "fib" (func 4))
  (export "fib2" (func 5))
  (export "orig$fib" (func 2))
  (export "orig$fib2" (func 3)))
