---
name: ZLMediaKit Configuration Translation Guidelines
description: Guidelines, standard terminology, and anti-patterns for translating ZLMediaKit configuration files and documentation from Chinese to English.
---

# ZLMediaKit Translation Rules & Skills

This document serves as the absolute source of truth for translating ZLMediaKit configuration files (`config.ini`), documentation, and comments from Chinese to English. Any AI agent (Claude Code, Gemini, etc.) MUST read and apply these rules before performing any translation tasks in this repository.

## 1. Core Philosophy

- **CBD Principle (Clarity, Brevity, Directness)**: Technical documentation must be clear, concise, and direct.
- **Native & Professional**: Emulate the documentation style of top-tier open-source projects (e.g., Nginx, WebRTC). Discard literal word-for-word translations. Restructure sentences if it makes the English sound more natural to a native speaker.

## 2. Standard Terminology Dictionary

**CRITICAL:** Always use the exact terminology specified below when encountering the corresponding Chinese concepts.

### Network & Architecture

- 源站 -> `Origin server` (NEVER use "Source station" or "Origin site")
- 溯源 (拉流) -> `Origin pull`
- 推流代理 / 拉流代理 -> `Publishing proxies` / `Pulling proxies` (Use gerunds for action consistency)
- 按需拉流 -> `On-demand stream pulling`
- 集群 -> `Cluster`
- 虚拟主机 -> `Virtual hosting`

### Video & Playback Experience

- 秒开 / 首屏秒开 -> `Zero-delay startup` or `Instant playback`
- 花屏 -> `Screen tearing` or `Visual artifacts`
- 卡顿 -> `Playback stuttering`

### Protocols & Media

- 切片 -> `Segment` or `Chunk` (e.g., HLS segment)
- 封装 / 打包 -> `Packaging` (e.g., RTP packaging)
- 负载 -> `Payload`

### General Technical Terms

- 处理 / 应对 (故障或异常情况) -> `Address` or `Handle` (Avoid "Resolve" unless the underlying root cause is completely eliminated)
- 正整数 / 非负整数 -> `non-negative integer` (Particularly when 0 is used to disable a feature)
- 鉴权 -> `Authentication`

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

- **Noun Clusters (Chinglish)**: Break up long strings of nouns using prepositions.
  - _Bad:_ `stream pulling reconnection`
  - _Good:_ `reconnection during stream pulling`
- **Translating Filler Words**: Delete unnecessary Chinese filler words like "该参数控制" (This parameter controls), "的逻辑" (the logic of), "该机制的目的是" (the purpose of this mechanism is). Get straight to the point.
  - _Bad:_ `This dictates the logic for...`
  - _Good:_ `Determines...` or directly state what it is.

## 5. Pre-translation Checklist

Before committing any translated text, verify against this checklist:

1. [ ] Did I use `Origin server` instead of `Source station`?
2. [ ] Are boolean flags phrased as `Whether to...`?
3. [ ] Are units (ms, bytes, seconds) clearly stated without being wordy?
4. [ ] Are there any awkward noun clusters missing prepositions?
5. [ ] Have all filler words ("logic", "mechanism", "controls") been removed for maximum brevity?
