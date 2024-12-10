define dso_local i32 @main() {
bb0:
        %r0 = alloca i32, align 4
        %r1 = alloca i32, align 4
        store i32 10, i32* %r0
        store  30, i32* %r1
        %r2 = load i32, i32* %r0
        %r4 = sub nsw i32 %r2, %r3
        %r5 = load i32, i32* %r1
        %r6 = add nsw i32 %r4, %r5
        %r8 = add nsw i32 %r6, %r7
        br label %bb1
bb1:
        %r9 = phi i32 [ %r8,%bb0 ], [ 0,%bb2 ]
        ret i32 %r9
bb2:
        br label %bb1
}