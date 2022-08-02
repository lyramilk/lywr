(module
  (type (;0;) (func (param i32 i64 i64 i64 i64)))
  (type (;1;) (func))
  (type (;2;) (func (param i64) (result i64)))
  (import "env" "__multi3" (func (;0;) (type 0)))
  (import "env" "__stack_pointer" (global (;0;) (mut i32)))
  (import "env" "__memory_base" (global (;1;) i32))
  (import "env" "memory" (memory (;0;) 1))
  (func (;1;) (type 1)
    nop)
  (func (;2;) (type 2) (param i64) (result i64)
    (local i32 i64 i64 i64 i64 i64)
    global.get 0
    i32.const 32
    i32.sub
    local.tee 1
    global.set 0
    i64.const 2
    local.set 6
    block  ;; label = @1
      local.get 0
      i64.const 2
      i64.gt_u
      if  ;; label = @2
        i64.const -9223372036854775807
        local.set 3
        i64.const 1
        local.set 4
        i64.const -1
        local.set 5
        loop  ;; label = @3
          local.get 1
          i32.const 16
          i32.add
          local.get 2
          i64.const 0
          local.get 5
          i64.const 0
          call 0
          local.get 1
          local.get 4
          i64.const 0
          local.get 2
          i64.const 0
          call 0
          local.get 4
          local.get 1
          i64.load offset=8
          i64.const 63
          i64.shl
          local.get 1
          i64.load
          i64.const 1
          i64.shr_u
          i64.or
          i64.add
          local.get 3
          i64.add
          local.get 2
          local.get 1
          i64.load offset=24
          i64.const 63
          i64.shl
          local.get 1
          i64.load offset=16
          i64.const 1
          i64.shr_u
          i64.or
          i64.add
          i64.add
          local.set 3
          local.get 5
          i64.const 1
          i64.add
          local.set 5
          local.get 2
          i64.const 1
          i64.add
          local.set 2
          local.get 4
          i64.const 1
          i64.add
          local.set 4
          local.get 6
          i64.const 1
          i64.add
          local.tee 6
          local.get 0
          i64.ne
          br_if 0 (;@3;)
        end
        br 1 (;@1;)
      end
      global.get 1
      local.get 0
      i32.wrap_i64
      i32.const 3
      i32.shl
      i32.add
      i64.load
      local.set 3
    end
    local.get 1
    i32.const 32
    i32.add
    global.set 0
    local.get 3)
  (export "__wasm_call_ctors" (func 1))
  (export "plus" (func 2))
  (data (;0;) (global.get 1) "\00\00\00\00\00\00\00\00\01\00\00\00\00\00\00\80\01\00\00\00\00\00\00\80"))
