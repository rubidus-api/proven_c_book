What we have learned so far is the *language*. Yet the accidents a C program in
the field actually runs into arise mostly not in the language but in the
*library* — string functions that do not take a size, the convention of reporting
failure with a single value, functions that return a static buffer, character
classification whose answer changes with the locale.

Chapter 58 spread out the map of the terrain, so in this part we walk each of its
regions ourselves. Taking the headers one by one, we look less at *what is there*
than at *where one slips*. Lists of functions are compressed into tables, and the
pages are spent on the traps and how to mend them.

The last three chapters have a slightly different grain. The tools C11 and C23
newly brought in — atomic operations, checked arithmetic, and the words promoted
from macros to keywords — get a chapter each. It is the place for learning *the
answers that have newly appeared*, not how to avoid the old traps.

There is one knack to reading it. This part may be read straight through, but it
is by nature a part to *look things up in*. Open the chapter for the header you
are using now and skim only the summary tables and the counterexamples, and it
already pays.
