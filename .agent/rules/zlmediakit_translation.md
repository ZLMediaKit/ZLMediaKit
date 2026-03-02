---
name: ZLMediaKit Configuration Translation Guidelines
description: Definitive guidelines, contextual awareness strategies, standard terminology, and anti-patterns for translating ZLMediaKit files from Chinese to English.
---

# ZLMediaKit Translation Rules & Skills

This document serves as the absolute source of truth for translating ZLMediaKit configuration files (`config.ini`), documentation, and comments from Chinese to English. Any AI agent (Claude Code, Gemini, etc.) MUST thoroughly read and apply these rules before performing any translation tasks in this repository.

## 1. Core Philosophy (The "Surface")

Translating ZLMediaKit requires moving beyond literal word-for-word translation (the "points") into understanding the underlying technical mechanisms (the "surface").

- **Technical Contextualization**: Before translating a comment, identify the specific domain of the configuration block.
  - `[http] / [api]`: Use standard Web developer terminology (e.g., `Request/Response`, `HTTP Headers`, `X-Forwarded-For`).
  - `[general] / System Configs`: Use low-level OS and Socket I/O terminology (e.g., `Write coalescing`, `buffer allocation`). If dropping unused connections, use `ignore` or `skip` rather than the destructive `abandon`.
  - `Media Streaming (RTSP/RTMP/RTC/HLS)`: Use industry-standard multimedia terms (e.g., `Visual artifacts` instead of screen tearing, `Grace period` instead of reconnection timeout).
- **CBD Principle (Clarity, Brevity, Directness)**: Technical documentation must be clear, concise, and direct. Emulate the documentation style of top-tier open-source projects (e.g., Nginx, WebRTC).
- **Action-Result Paradigm**: When a Chinese comment arbitrarily describes "this mechanism's logic," convert it into a direct Action-Result statement: `Setting this to 0 disables X and allows Y.` Explain _what happens_ when a user changes the value, not just the abstract theory.

## 2. Standard Terminology Dictionary

**CRITICAL:** Always use the exact terminology specified below when encountering the corresponding Chinese concepts.

### Network & Architecture

- 源站 -> `Origin server` (NEVER use "Source station" or "Origin site")
- 溯源 (拉流) -> `Origin pull`
- 推流代理 / 拉流代理 -> `Publishing proxies` / `Pulling proxies` (Use gerunds for action consistency)
- 按需拉流 -> `On-demand stream pulling`
- 集群 -> `Cluster`
- 虚拟主机 -> `Virtual hosting`
- 推流断开后的超时等待 -> `Grace period for publisher reconnection`

### Video & Playback Experience

- 秒开 / 极速秒开 -> `Instant playback` or `Zero-delay startup`
  - _Example (级联秒开):_ `Instant playback for cascaded streams`
- 花屏 -> `Visual artifacts` or `glitches` (NEVER use "Screen tearing", which is a monitor sync issue)
- 卡顿 -> `Playback stuttering`

### System I/O & HTTP

- 合并写 -> `Write coalescing` or `Batch writing` (NEVER use "Merged write")
- 请求和回复 -> `Request and Response` (Avoid "reply" in HTTP contexts)
- 在代理后方获取真实IP -> `Extract the real client IP when behind a proxy (e.g., via X-Forwarded-For)` (Avoid "Captures the true client IP...")

### Protocols & Media

- 切片 -> `Segment` or `Chunk` (e.g., HLS segment)
- 封装 / 打包 -> `Packaging` (e.g., RTP packaging)
- 负载 -> `Payload`

### General Technical Terms

- 处理 / 应对 (故障或异常情况) -> `Address` or `Handle` (Avoid "Resolve" unless the underlying root cause is completely eliminated)
- 正整数 / 非负整数 -> `non-negative integer` (Particularly when 0 is used to disable a feature)
- 鉴权 -> `Authentication`
- 忽略 (非严重的丢弃) -> `Ignore` or `Skip` (Reserve "abandon" for critical, unrecoverable states)

## 3. Standard Sentence Patterns

To maintain a consistent tone across the configuration file, strictly adhere to these sentence structures:

- **Boolean Switches (0/1)**: Start with `Whether to [action]...`
  - _Example:_ `Whether to enable virtual hosting.`
- **Limits & Thresholds**: Start with `Maximum time/duration/size [in unit] for...`
  - _Example:_ `Maximum wait duration for playback requests in milliseconds.`
- **Event Callbacks / Webhooks**: Start with `Callback triggered when...` or simply `[Event Name] event.`
  - _Example:_ `Callback triggered when RTP sending is passively closed.`
- **Disabling Features**: When 0 disables a feature, do not fully translate "设置为0则关闭". Use a succinct suffix: `(0 to disable)`.
- **Units**: Place units in parentheses or immediately after the noun.
  - _Example:_ `Cache size (in bytes)` or `Timeout in seconds`.

## 4. Anti-Patterns to Avoid

- **De-jargonization Failure**: Failing to decode Chinese streaming slang (黑话) before translating.
  - _Bad:_ `cascading zero-delay startups`
  - _Good:_ `instant playback for cascaded streams`
- **Noun Clusters (Chinglish)**: Break up long strings of nouns using prepositions.
  - _Bad:_ `stream pulling reconnection`
  - _Good:_ `reconnection during stream pulling`
- **Translating Filler Words**: Delete unnecessary Chinese filler words like "该参数控制" (This parameter controls), "的逻辑" (the logic of), "该机制的目的是" (the purpose of this mechanism is). Get straight to the point.
  - _Bad:_ `This dictates the logic for...`
  - _Good:_ `Determines...` or directly state what it is.

## 5. Pre-translation Checklist

Before committing any translated text, verify against this checklist:

1. [ ] **Context Check:** Did I tailor the vocabulary to the specific module (e.g., I/O terms for `general`, HTTP terms for `api`)?
2. [ ] Did I use `Origin server` instead of `Source station`?
3. [ ] Are boolean flags phrased as `Whether to...` and limits defined with clear `(in unit)` markings?
4. [ ] Did I unpack Chinese jargon (e.g., interpreting "级联秒开" as an action rather than a literal noun)?
5. [ ] Is the Action-Result logic clear (i.e., "Setting this to 0 does X", rather than translating vague explanations)?
6. [ ] Have all filler words ("logic", "mechanism", "controls") been removed for maximum brevity?
