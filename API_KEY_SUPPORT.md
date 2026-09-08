# API Key Support for External LLM Providers

## Implementation Summary

Added support for Claude and Google Antigravity API providers alongside existing Ollama support.

## New Features

### 1. Configuration Extension (src/core/types.h)
- Added `claude_api_key`, `claude_model` fields
- Added `google_agy_api_key`, `google_agy_model`, `google_agy_endpoint` fields
- Renamed `api_url` to `ollama_api_url` for clarity
- API type now supports: "ollama", "claude", "google-agy"

### 2. Configuration Parser Updates (src/core/ConfigParser.h/cpp)
- New `apply_env_overrides()` function
- Environment variable fallback support:
  - `CLAUDE_API_KEY` → claude_api_key
  - `GOOGLE_AGY_API_KEY` → google_agy_api_key
  - `GOOGLE_AGY_MODEL` → google_agy_model
- Config file values can be overridden by environment variables (secure approach)
- Parses new config keys: claude_model, google_agy_model, google_agy_api_key, etc.

### 3. Claude API Client (src/llm/ClaudeClient.h/cpp)
- Implements LLMClient interface for Anthropic Claude
- Supported models:
  - claude-3-opus-20240229
  - claude-3-sonnet-20240229
  - claude-3-haiku-20240307
  - claude-3-5-sonnet-20241022 (default)
- Features:
  - HTTP POST to Anthropic Messages API
  - Token counting from response metadata
  - Streaming support (fallback to non-streaming for now)
  - Automatic env var lookup if no key provided
  - Thread-safe CURL implementation

### 4. Google Antigravity Client (src/llm/GoogleAntigravityClient.h/cpp)
- Implements LLMClient interface for Google Antigravity
- Configurable endpoint (default: https://agy.googleapis.com/v1beta1)
- Features:
  - HTTP POST to Google API
  - API key query parameter authentication
  - Custom endpoint support for different regions/versions
  - Model support from config
  - Thread-safe CURL implementation

### 5. ClientFactory Enhancement (src/llm/LLMClient.h/cpp)
- Original `create()` method updated to support claude and google-agy
- New `create_from_config()` method for detailed configuration
  - Takes individual parameters: api_type, api_keys, models, etc.
  - Easier integration with Configuration struct
  - Backward compatible with existing ollama factory

## Configuration File Format

```ini
# API backend selection
api_type=ollama        # or "claude" or "google-agy"

# For Ollama
ollama_api_url=http://localhost:11434/api/chat

# For Claude
claude_model=claude-3-5-sonnet-20241022
claude_api_key=sk-ant-...  # or use CLAUDE_API_KEY env var

# For Google Antigravity
google_agy_model=claude-3-5-sonnet-20241022
google_agy_api_key=...      # or use GOOGLE_AGY_API_KEY env var
google_agy_endpoint=https://agy.googleapis.com/v1beta1
```

## Environment Variables

Provides secure way to pass sensitive API keys without storing in config files:

```bash
# Claude
export CLAUDE_API_KEY="sk-ant-..."

# Google Antigravity
export GOOGLE_AGY_API_KEY="..."
export GOOGLE_AGY_MODEL="claude-3-5-sonnet-20241022"
export GOOGLE_AGY_ENDPOINT="https://custom-endpoint/v1beta1"
```

Priority: Config file value (if set) → Environment variable → Default

## Security Considerations

1. **API Keys**: 
   - Never logged or printed to console
   - Sourced from environment variables first (more secure)
   - Config file should have restricted permissions (644 or 600)

2. **HTTPS Only**:
   - Both Claude and Google Antigravity APIs use HTTPS
   - CURL validates certificates by default

3. **Timeout Protection**:
   - Default 120-second timeout on all API requests
   - Configurable per client

4. **Error Handling**:
   - HTTP errors included in response error field
   - API errors don't crash the application

## Build Changes

- Added src/llm/ClaudeClient.cpp to CXX_SRCS
- Added src/llm/GoogleAntigravityClient.cpp to CXX_SRCS
- Added compilation rules for both new clients in Makefile

## Testing Performed

✓ Build test: All sources compile without errors (warnings only from existing code)
✓ Configuration struct accepts new fields
✓ ConfigParser::apply_env_overrides() defined and compilable
✓ ClientFactory can instantiate both Claude and Google Antigravity clients
✓ LLMClient interface properly implemented by both new clients

## Next Steps (Not Yet Implemented)

1. **Main Integration**:
   - Update main() to use create_from_config() factory method
   - Pass Configuration struct to factory instead of hardcoded values
   - Support --api-type CLI flag

2. **Configuration Documentation**:
   - Update config/council.conf with API key examples
   - Add security best practices guide

3. **Streaming Implementation**:
   - Implement true streaming for Claude (server-sent events)
   - Implement streaming for Google Antigravity

4. **Testing**:
   - Unit tests for ClaudeClient and GoogleAntigravityClient
   - Integration tests with actual APIs (requires valid keys)
   - Configuration loading tests with env vars

5. **Error Handling**:
   - Better JSON parsing for error messages
   - Retry logic for transient failures
   - Rate limit handling

## Files Modified/Created

### Modified
- src/core/types.h: Extended Configuration struct
- src/core/ConfigParser.h/cpp: Added env var support
- src/llm/LLMClient.h/cpp: Extended factory, added new clients
- Makefile: Added new source files and compilation rules

### Created
- src/llm/ClaudeClient.h/cpp: Anthropic Claude API wrapper
- src/llm/GoogleAntigravityClient.h/cpp: Google Antigravity API wrapper

## API Key Provider Information

### Claude API
- Provider: Anthropic
- Documentation: https://docs.anthropic.com/en/api/messages
- API Key: Get from https://console.anthropic.com
- Models: claude-3-opus, claude-3-sonnet, claude-3-haiku, claude-3-5-sonnet (latest)

### Google Antigravity
- Provider: Google Cloud
- Documentation: (varies by region/version)
- API Key: Get from Google Cloud Console
- Endpoint: https://agy.googleapis.com/v1beta1 (default)
