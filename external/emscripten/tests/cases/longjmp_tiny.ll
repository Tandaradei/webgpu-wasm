; ModuleID = 'tests/hello_world.bc'
target datalayout = "e-p:32:32-i64:64-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

@_ZL3buf = internal global [20 x i16] zeroinitializer, align 2
@.str = private unnamed_addr constant [13 x i8] c"hello world\0A\00", align 1
@.str1 = private unnamed_addr constant [6 x i8] c"more\0A\00", align 1

define i32 @main() {
  %retval = alloca i32, align 4
  store i32 0, i32* %retval
  %call = call i32 @setjmp(i16* getelementptr inbounds ([20 x i16], [20 x i16]* @_ZL3buf, i32 0, i32 0)) returns_twice  %tobool = icmp ne i32 %call, 0  br i1 %tobool, label %if.else, label %if.then
if.then:                                          ; preds = %entry
  %call1 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([13 x i8], [13 x i8]* @.str, i32 0, i32 0))  call void @longjmp(i16* getelementptr inbounds ([20 x i16], [20 x i16]* @_ZL3buf, i32 0, i32 0), i32 10)  br label %if.end
if.else:                                          ; preds = %entry
  %call2 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str1, i32 0, i32 0))  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  ret i32 0}

declare i32 @setjmp(i16*) returns_twice

declare i32 @printf(i8*, ...)

declare void @longjmp(i16*, i32)


