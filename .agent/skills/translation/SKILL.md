---
name: Project General Translation & Terminology Guidelines
description: Definitive guidelines, contextual awareness strategies, standard terminology, and comment formatting rules for translating code, configurations, and documentation from Chinese to English in this repository.
---

# 🤖 Systemic Translation & Terminology Instructions for AI Agents

This document is the absolute source of truth and **Standard Operating Procedure (SOP)** for translating Chinese comments, configurations, and documentation into English within this repository.

**ATTENTION AI AGENTS:** You are NOT merely translating words; you are executing a systematic algorithm to localize complex streaming media and networking concepts. Do not rely solely on "passive reading" or "translation memory." You MUST follow the rigid workflow outlined below.

---

## Phase 1: Contextual Anchoring (MANDATORY BEFORE TRANSLATION)

Before translating any block of text, you must explicitly anchor yourself to the specific technical domain. **Literal translation of Chinese industry slang (黑话) is strictly prohibited.**

1. **Identify the Domain:** Look at the module or configuration section (e.g., `[rtp_proxy]`, `[http]`, `[general]`, `[hls]`).
2. **Setup the Mental Lexicon:**
   - If `[api]/[http]`: Anchor to standard REST API and Web server concepts (e.g., `Requests/Responses`, `CORS`, `Forwarded IPs`).
   - If `Network I/O / [general]`: Anchor to socket programming and OS-level terms (e.g., `Write coalescing`, `Buffers`, `File handles`).
   - If `Media Streaming (RTSP/RTMP/RTC)`: Anchor to multimedia transport concepts (e.g., `GOP`, `Payload`, `B-frames`, `Jitter`, `Visual artifacts`).
3. **Verification-Driven Translation:** If you encounter a Chinese term that sounds colloquial or metaphoric (e.g., “花屏” - flowered screen, “秒开” - open in seconds, “溯源” - trace back to origin), **DO NOT guess or translate literally**. Ask yourself: _"How do top-tier English open-source projects (FFmpeg, WebRTC, Nginx) refer to this specific technical phenomenon?"_

---

## Phase 2: Structural Translation & Anti-Pattern Detection

LLMs naturally tend to follow the grammatical structure of the source text. Chinese technical writing often uses sprawling sentences and explanatory fillers. You must actively break these patterns.

### 🚫 Rule 1: The "Action-Result" Paradigm

- **Trigger:** When the Chinese text says "设置为0关闭此特性" (Setting this to 0 disables this feature) or "打开此选项会导致..." (Turning this on causes...).
- **Execution:** Force your output to use the exact structure: `Setting this to [Value] disables [Feature] and allows [Consequence].` Do NOT translate explanatory filler like "This mechanism's logic dictates that...".

### 🚫 Rule 2: Sub-clause Elimination (No "Chinglish")

- **Trigger:** Long noun clusters or overly personified system descriptions (e.g., "服务器会认为这个流是断开的" - The server will think this stream is disconnected).
- **Execution:** Use direct, objective voice: `The stream is considered disconnected.` or `The system drops the stream.`

### 🚫 Rule 3: Clarifying Ambiguous Actions

- **Trigger:** The word `忽略` (Ignore/Skip) vs. `丢弃/放弃` (Abandon/Drop).
- **Execution:** Use `Ignore` or `Skip` for non-critical timeouts (e.g., waiting for a track to be ready). Reserve `Abandon`, `Drop` or `Disconnect` only for fatal errors or closed sockets.

### 🚫 Rule 4: Zero Information Loss & Causal Reconstruction

- **Trigger:** When condensing text for native flow, or translating complex caveats (e.g., parenthetical conditions, "而不是" / instead of, side-effects).
- **Execution:** You may reorganise syntax to sound professional, but you MUST NOT drop crucial qualifiers, modifiers, or side effects. If a Chinese config says "instead of returning X via hook", the English translation must explicitly mention "returning X". Information completeness supersedes structural brevity.

### 🚫 Rule 5: The Golden Balance (Zero Info Loss vs. Native Phrasing)

- **The Core Conflict:** You must achieve **Zero Information Loss** WITHOUT resorting to **Chinglish** or literal word-for-word translation.
- **What "Information" Means:** "Retaining information" means capturing 100% of the **technical causality**, **side-effects**, **prerequisites**, and **system boundaries** present in the Chinese text.
- **What "Information" DOES NOT Mean:** It does NOT mean preserving the Chinese grammatical structure, literal phrasing, or colloquialisms (啰嗦句子和字面用词).
- **Execution (The Top-Down Conceptual Approach):**
  1. **Contextual Override:** Never translate a noun literally if the surrounding constraints (e.g., units like "seconds", prefixes, or the specific protocol) dictate a domain term. For example, if a setting is measured in "seconds", the Chinese word "大小" (size) MUST logically translate to `Duration` or `Interval`, **NEVER** `Size`.
  2. **Conceptual Compression:** When faced with a sprawling, explanatory Chinese sentence, distill the _technical payload_ and express it using concise, standard industry jargon.
     - _Anti-pattern (Literal/Chinglish):_ `After disabling the traditional authentication mode, you must first call the API to log in. Upon success, a cookie will be set, and all APIs can be accessed without restriction as long as the cookie is valid.`
     - _Pro-pattern (Native/Jargon):_ `When disabled, users must first call /index/api/login. Upon success, a cookie auth token is set for subsequent requests.` (Using "subsequent requests" efficiently compresses the lengthy Chinese explanation).
  3. **Technical Abstraction:** Recognize standard operations (e.g., "拉流再推流"). Do not translate the physical actions (`pulling and then pushing`); translate the abstract technical process (`re-publishing` or `re-encoding`).

### 🚫 Rule 6: Anti-Summarization (Strict Boolean & Causality Preservation)

- **Trigger:** When applying Conceptual Compression (Rule 5) to a text block containing conditionals or explanations.
- **The Core Conflict:** _Compression_ reduces word count by using jargon. _Summarization_ drops critical logic. **Summarization is strictly forbidden.**
- **Execution (The Boolean Mapping Rule):**
  1. Treat Chinese comments like code blocks. Extract all `IF/THEN/ELSE` branches, prerequisites, and root causes before translating.
  2. If the original text states a "success" path and a "failure" path, the English translation MUST explicitly state both paths. You cannot compress them into a single vague outcome.
  3. If the original text states _why_ a feature exists (the exact cause or defect being prevented), the English translation MUST explicitly state that exact cause. You cannot compress it into generic "to improve performance" or "to prevent errors."
  4. Perform a **Reverse Mapping Check**: After writing the English sentence, ask yourself—"If I reverse-compile this English back to Chinese, would any `IF` conditions or edge-case explanations be missing?" If yes, rewrite it completely to restore the dropped logic.

---

## Phase 3: The Hardcoded Terminology Dictionary

**CRITICAL:** When translating, if you encounter these Chinese concepts, you MUST use the exact, first provided English term. **Do not mix or alternate synonyms.**

### Network & Architecture

- 源站 -> `Origin server`
- 溯源 (拉流) -> `Origin pull`
- 推流代理 / 拉流代理 -> `Publishing proxies` / `Pulling proxies`
- 按需拉流 -> `On-demand stream pulling`
- 集群 -> `Cluster`
- 推流断开后的超时等待 -> `Grace period for publisher reconnection`

### Video & Playback Experience

- 秒开 / 极速秒开 -> `Instant playback (zero-delay startup)` (e.g., 级联秒开 -> `Instant playback for cascaded streams`)
- 花屏 -> `Visual artifacts (glitches)` _(NEVER use "Screen tearing", which is a hardware V-sync issue)_
- 卡顿 -> `Playback stuttering`

### System I/O & HTTP

- 合并写 -> `Write coalescing` _(NEVER use "Merged write")_
- 请求和回复 -> `Requests and Responses` _(Avoid "Replies")_
- 在代理后方获取真实IP -> `Extract the real client IP when behind a proxy (e.g., via X-Forwarded-For)`

### General Technical Terms

- 切片 -> `Segment` (e.g., HLS segment)
- 封装 / 打包 -> `Packaging`
- 负载 -> `Payload`
- 鉴权 -> `Authentication`
- 处理 / 应对 (故障) -> `Handle` or `Address`

---

## Phase 4: Strict Formatting Rules (CRITICAL)

When translating comments inside code files (`.cpp`, `.h`) or configs (`.ini`), apply these hard constraints:

1. **Bilingual Retention:** Unless explicitly instructed to delete Chinese, **ALWAYS retain the original Chinese comments**.
2. **Bottom Placement:** Place the English translation immediately **below** the Chinese line or block.
3. **Block Uniformity:** Do NOT translate line-by-line (`ZH-EN-ZH-EN`). If a Chinese comment is a 3-line block, output it as a 3-line Chinese block followed by a 3-line English block.

```cpp
/*
 * 这里是第一行中文描述。
 * 这里是第二行中文补充。
 */
/*
 * This is the English translation of the first line.
 * This is the English translation of the second line.
 */
```

---

## Phase 5: The Post-Translation Verification Workflow (DO NOT SKIP)

If you are asked to review or update translations in a long file, **you cannot rely solely on passive reading**. You MUST execute this workflow:

1. **Active Scan (Regex/Search):** Before reading the document, use file search tools to actively scan for known anti-patterns in the current English text (e.g., search for `Screen tearing`, `Merged write`, `Replies`, `Source station`). Fix them immediately.
2. **Format Review:** Scan for `ZH-EN-ZH-EN` interleaving and fix it to block format.
3. **Blind English Review:** After translating, hide the Chinese text from your mental context. Read _only_ your English output constraint: _Does this sound like a snippet from the official Nginx or WebRTC manuals? Is it concise (CBD: Clarity, Brevity, Directness)?_ If it sounds like a literal word-for-word translation, rewrite it natively.
