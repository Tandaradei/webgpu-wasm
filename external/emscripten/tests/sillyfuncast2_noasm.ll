; ModuleID = 'tests/hello_world.bc'
target datalayout = "e-p:32:32-i64:64-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

@.str = private unnamed_addr constant [15 x i8] c"hello, world!\0A\00", align 1 ; [#uses=1 type=[15 x i8]*]

define void @doit(i32 %one, i32 %two) {
  %call = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([15 x i8], [15 x i8]* @.str, i32 0, i32 0)) ; [#uses=0 type=i32]
  ret void
}

define i32 @main() {
entry:
  %retval = alloca i32, align 4                   ; [#uses=1 type=i32*]
  store i32 0, i32* %retval
  call i32 bitcast (void (i32, i32)* @doit to i32 (i32, i64)*)(i32 0, i64 0) nounwind
  call void bitcast (void (i32, i32)* @doit to void (i32, float)*)(i32 0, float 0.0) nounwind
  ret i32 1
}

declare i32 @printf(i8*, ...)

