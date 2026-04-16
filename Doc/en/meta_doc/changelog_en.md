# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Prediction candidates now open while composing numeric input, and consecutive digits continue composing instead of being treated only as candidate shortcuts.
- Regression tests were added for uppercase romaji input, half-width digit handling, hyphen punctuation handling, and brace input handling.
- Shifted alphabetic input now preserves uppercase letters until romaji conversion resolves them.
- Numeric input is now normalized to half-width digits during input.
- Full-width direct input now applies only to ASCII letters. Digits remain half-width in direct input and can appear as full-width forms through conversion candidates instead.
- A hyphen (`-`) now commits as the long vowel mark (`ー`) in Hiragana input mode.
- Braces (`{` and `}`) continue to commit as half-width characters.
- For the feature set targeted at v1.0.0 usability, prediction ordering has been strengthened to better follow Japanese grammar. Candidate ranking now reacts more naturally to preceding particles and sentence-ending expressions when choosing between nouns, verbs, particles, and endings.
- In addition to single particles such as `を`, `の`, `は`, `が`, `も`, `に`, `で`, `へ`, and `と`, follow-up candidates after compound particles such as `には`, `では`, `とは`, `ので`, and `のに` are also adjusted. Raw reading candidates and romaji or katakana fallback candidates are less likely to appear near the top when they do not fit the context.
- While composing, shorter readings now use a smaller prediction candidate window and expand as the reading becomes longer, reducing noise early without removing useful candidates for longer input.
- Initial conversion segmentation now considers part-of-speech flow between neighboring segments, making common patterns such as `noun + particle`, `verb stem + ます`, and `noun + です` easier to reach from the initial state.
- During conversion, changing an earlier segment now invalidates and re-evaluates following segment candidates under the updated context, and the next segment candidate set is prefetched to improve responsiveness in continuous conversion.
