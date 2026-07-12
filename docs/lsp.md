---
layout: ../../layouts/DocsLayout.astro
title: Language Server (LSP)
---

# Language Server (hok-lsp)

`hok-lsp` is the language server for Hokkaido, providing IDE features for any editor
that supports the [Language Server Protocol](https://microsoft.github.io/language-server-protocol/)
(LSP): VS Code, Neovim, Helix, Emacs, Sublime Text, and more.

## Features

| Feature | Description |
|---------|-------------|
| **Diagnostics** | Parse errors reported inline as you type |
| **Hover** | Shows symbol kind and name on hover |
| **Completion** | Keyword suggestions + symbols from all open documents |
| **Go to Definition** | Navigate to symbol declarations |
| **Find References** | Find all occurrences of a symbol in the current file |
| **Document Symbols** | Outline of functions, variables, structs, enums, traits |
| **Incremental Parsing** | After the first full parse, edits re-parse only the affected portion |

## Building

`hok-lsp` is built as part of the Hokkaido CMake project:

```sh
mkdir build && cd build
cmake .. && make hok-lsp
```

Or with Nix:

```sh
nix build github:hokkaido-lang/hokkaido
```

The binary is installed alongside `hokkaido` when using `hokup` or `nix profile install`.

## Editor setup

### Neovim (vim.lsp)

Add to your init.lua:

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

Use a generic LSP client extension and configure:

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

`hok-lsp` communicates over **stdio** using the standard LSP JSON-RPC transport
(`Content-Length` headers). It supports:

- `textDocument/didOpen` / `didChange` / `didClose`
- `textDocument/hover`
- `textDocument/completion`
- `textDocument/definition`
- `textDocument/references`
- `textDocument/documentSymbol`
