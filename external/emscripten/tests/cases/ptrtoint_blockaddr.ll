; ModuleID = 'tests/hello_world.bc'
target datalayout = "e-p:32:32-i64:64-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

@.str = private constant [15 x i8] c"hello, world!\0A\00", align 1 ; [#uses=1]

define i32 @main() {
  %a199 = trunc i8 1 to i1                        ; [#uses=1]
  br i1 %a199, label %label555, label %label569

label555:                                     ; preds = %353
  br label %label569
                                                  ; No predecessors!
  br label %label569

label569:                                     ; preds = %555
  %a333 = call i32 @printf(i8* getelementptr inbounds ([15 x i8], [15 x i8]* @.str, i32 0, i32 0)) ; [#uses=0]
  ; this should compile ok
  store i32 ptrtoint (i8* blockaddress(@main, %label569) to i32), i32* bitcast (i8* getelementptr inbounds ([15 x i8], [15 x i8]* @.str, i32 0, i32 0) to i32*), align 8
  ret i32 0
}

declare i32 @printf(i8*)

