# hok-lsp — Hokkaido Language Server

`hok-lsp` is an LSP (Language Server Protocol) implementation for the Hokkaido programming language, providing IDE features for editors that support LSP (VS Code, Neovim, Emacs, Helix, etc.).

## Features

- **Diagnostics** — parse errors reported inline as you type
- **Hover** — shows symbol kind and name on hover
- **Completion** — keyword suggestions + symbols from all open documents
- **Go to Definition** — navigate to symbol declarations
- **Find References** — find all occurrences of a symbol in the current file
- **Document Symbols** — outline of functions, variables, structs, enums, traits
- **Incremental Parsing** — after the first full parse, edits re-parse only the affected portion of the file

## Building

Built as part of the Hokkaido CMake project:

```sh
mkdir build && cd build
cmake .. && make hok-lsp
```

Or with Nix (installed to `$out/bin/hok-lsp`):

```sh
nix build github:hokkaido-lang/hokkaido
```

## Usage

### Neovim (via `vim.lsp`)

```lua
vim.api.nvim_create_autocmd('FileType', {
  pattern = 'hk',
  callback = function()
    vim.lsp.start({
      name = 'hok-lsp',
      cmd = { 'hok-lsp' },
    })
  end,
})
```

### Helix

Add to `~/.config/helix/languages.toml`:

```toml
[[language]]
name = "hokkaido"
scope = "source.hk"
file-types = ["hk"]
language-servers = ["hok-lsp"]

[language-server.hok-lsp]
command = "hok-lsp"
```

### VS Code

Create a simple extension or use a generic LSP client:

```json
{
  "languages": [{
    "id": "hokkaido",
    "extensions": ["hk"]
  }],
  "languageServer": {
    "command": "hok-lsp"
  }
}
```

## Protocol

Communicates over **stdio** using the standard LSP JSON-RPC transport (`Content-Length` headers).

Supports `textDocument/didOpen`, `didChange`, `didClose`, `hover`, `completion`, `definition`, `references`, and `documentSymbol`.

## Architecture

- **`lsp.h`** — type definitions (`LSPPosition`, `LSPRange`, `LSPDiagnostic`, `LSPSymbol`, `LSPCompletionItem`, `DeclRange`, `LSPDocument`) and `LSPServer` class
- **`lsp.cpp`** — JSON-RPC message framing, protocol handlers, symbol index construction, incremental parsing
- **`lsp_main.cpp`** — entry point that instantiates `LSPServer` and calls `run()`
