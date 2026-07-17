refectoring all sapporo,sapporo-cli and improve it for more developer experience and fix all bugs and update docs   
make Sapporo use hokkaido stdlib for memory management if hokkaido stdlib has error fix error in hokkaido lib if something has error fix it  
refactoring all about Sapporo and improve and fix all bugs
after fix and refectoring all create new branch submit a pull request


known bugs
todo app in @sapporo/docs/examples.md doesn't work - FIXED

completed in this branch:
- Fixed broken Todo app example (removed mutable global, simplified to working add/clear-all)
- Fixed std/mem.hk: removed libc dependency (memcpy/memset), pure implementations for WASM freestanding
- Added new stdlib functions: mem_find, mem_contains, find_int, count_int, sum_ints
- Integrated stdlib into sapporo.hk (import "std", re-export memory ops)
- Refactored sapporo.js: added element ID caching (fixes registry leak), auto-reset bump allocator, memory_stats function
- Added missing sapporo.hk wrappers: set_children, memory_stats, copy_text
- Refactored sapporo-cli: deduplicated scaffolding (new.rs/init.rs -> scaffold.rs), added --verbose flag, improved error handling (captures stderr from compiler/linker), simplified tool discovery
- Updated documentation: api.md (new APIs), docs.md (caching), callbacks.md, examples.md, cli README
