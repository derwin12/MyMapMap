# MyMapMap MCP Server

MyMapMap embeds a [Model Context Protocol](https://modelcontextprotocol.io) (MCP) server
that lets AI assistants (Claude Desktop, Claude Code, or any MCP-compatible client) inspect
and control the running application over a local HTTP connection.

## Default endpoint

```
http://localhost:49452/mcp
```

All calls are `POST` requests with `Content-Type: application/json`.

## Changing the port

Open **Preferences → Controls → MCP Setup** and enter the desired port, then restart the
server (or relaunch the application). You can also pass `--mcp-port <n>` on the command
line.

## Connecting Claude Desktop

Add the following block to your `claude_desktop_config.json`
(`%APPDATA%\Claude\claude_desktop_config.json` on Windows,
`~/Library/Application Support/Claude/claude_desktop_config.json` on macOS):

```json
{
  "mcpServers": {
    "mymapmap": {
      "command": "npx",
      "args": [
        "-y",
        "mcp-remote",
        "http://localhost:49452/mcp"
      ]
    }
  }
}
```

Restart Claude Desktop after saving the file. MyMapMap must be running before Claude
Desktop connects.

> **Tip — Claude Code** users do not need the config file. Use the `/mapmap-mcp` skill
> or call the endpoint directly with `curl` as shown below.

## Quick test with curl

```bash
# 1. Initialize the session
curl -s -X POST http://localhost:49452/mcp \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}'

# 2. List all sources
curl -s -X POST http://localhost:49452/mcp \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"list_sources","arguments":{}}}'
```

## Available tools (summary)

| Category | Tools |
|----------|-------|
| Playback | `play`, `pause`, `rewind`, `set_fps`, `quit` |
| Project | `load_project`, `save_project`, `clear_project` |
| Sources | `create_color_source`, `create_media_source`, `create_folder_source`, `delete_source` |
| Layers | `create_layer`, `delete_layer`, `duplicate_layer`, `move_layer`, `set_layer_visible`, `set_layer_solo`, `set_layer_locked` |
| Geometry | `set_vertices`, `set_source_vertices`, `nudge_vertex` |
| Properties | `set_property` |
| Introspection | `get_state`, `list_sources`, `list_layers`, `get_source`, `get_layer` |
| Preview | `get_preview` |

For full argument details see `.claude/skills/mapmap-mcp.md` in the source tree.
