As stated in chapter 1, this book also has the purpose of making known a library
the author made. This is the place where that promise is kept — though the order is
exactly as was said at the start. The preceding forty-six chapters came with
standard C alone, and proven showed its face only once, at the place where the need
proved itself (chapter 42).

So this part too begins not with the tool but with the *problem*. Chapter 83
confirms with actually running code the five classes of defect at which C has
slipped in the same places for half a century, and the nine chapters that follow
point to the answer for each — installation and a first program (chapter 84), errors
that come as values (chapter 85), bytes and views (chapter 86), allocators
(chapter 87), strings (chapter 88), formatting and parsing (chapter 89), containers
and algorithms (chapter 90), files, time and random numbers (chapter 91), and
running things overlapped and environments with no OS (chapter 92).

One thing should be said in advance. The design this part will show — taking an
allocator as a parameter, returning failure as a value, views that carry their
length — was not invented by proven. Zig, Rust and recent C++ have moved in the
same direction, and there is considerable common ground about it. But *proven is
not that common ground itself: it is one attempt to implement it in C23.* It is
neither a standard nor a component the industry has adopted. That distinction
holds throughout this part — the direction has been verified in many places, this
implementation has not yet (chapter 92 sets down the supported range and the
stability honestly).

It is well to hold two purposes in reading. If you mean to use proven, this part is
the starting point; and even if you do not — read it as a case of following to the
end *how the dangers this book has taught are blocked by API design*. Whichever
library you choose, that eye is used just the same. That each section does not omit
"and so what is the price" is for the same reason — there is no free choice.
