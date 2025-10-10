# Phase 4: Streaming & Pipes - Parallel Execution Strategy

**Created**: 2025-10-10
**Status**: EXECUTING
**Total Tasks**: 25
**Estimated Completion**: 2-3 hours

## Executive Summary

Phase 4 integrates HTTP message classes (IncomingMessage, ServerResponse, ClientRequest) with the existing node:stream infrastructure. This enables streaming data handling, back-pressure management, and pipe() support for efficient data transfer.

### Key Integration Points

1. **IncomingMessage → Readable Stream** (Task 4.1)
   - Inherit from stream.Readable
   - Emit 'data' events when HTTP body chunks arrive
   - Support pause()/resume() for back-pressure
   - Enable pipe() to other Writable streams

2. **ServerResponse → Writable Stream** (Task 4.2)
   - Inherit from stream.Writable
   - Implement _write() for back-pressure handling
   - Support write() callbacks for flow control
   - Enable pipe() from Readable streams

3. **ClientRequest → Writable Stream** (Task 4.3)
   - Inherit from stream.Writable for request body
   - Support chunked Transfer-Encoding automatically
   - Implement _write() for request streaming
   - Handle large POST/PUT payloads efficiently

4. **Advanced Features** (Task 4.4)
   - Stream lifecycle management (destroy, error propagation)
   - State tracking (ended, finished, destroyed)
   - highWaterMark for buffer control
   - Error handling and recovery

## Dependency Analysis

### Task Groups (Sequential Execution Required)

```
Group 1: IncomingMessage (Task 4.1) [FOUNDATION]
  ├─ 4.1.1: Make Readable stream [CRITICAL PATH]
  ├─ 4.1.2: Emit 'data' events [DEPENDS: 4.1.1]
  ├─ 4.1.3: Emit 'end' event [DEPENDS: 4.1.2]
  ├─ 4.1.4: pause()/resume() [DEPENDS: 4.1.1]
  ├─ 4.1.5: pipe() support [DEPENDS: 4.1.1, 4.1.4]
  └─ 4.1.6: Test [DEPENDS: ALL]

Group 2: ServerResponse (Task 4.2) [DEPENDS: 4.1 complete]
  ├─ 4.2.1: Make Writable stream [CRITICAL PATH]
  ├─ 4.2.2: _write() implementation [DEPENDS: 4.2.1]
  ├─ 4.2.3: _final() implementation [DEPENDS: 4.2.2]
  ├─ 4.2.4: cork()/uncork() [DEPENDS: 4.2.1]
  ├─ 4.2.5: pipe() support [DEPENDS: 4.2.1, 4.2.2]
  └─ 4.2.6: Test [DEPENDS: ALL]

Group 3: ClientRequest (Task 4.3) [DEPENDS: 4.2 complete]
  ├─ 4.3.1: Make Writable stream [CRITICAL PATH]
  ├─ 4.3.2: _write() implementation [DEPENDS: 4.3.1]
  ├─ 4.3.3: _final() implementation [DEPENDS: 4.3.2]
  ├─ 4.3.4: Chunked encoding [DEPENDS: 4.3.2]
  ├─ 4.3.5: flushHeaders() [DEPENDS: 4.3.1]
  └─ 4.3.6: Test [DEPENDS: ALL]

Group 4: Advanced (Task 4.4) [DEPENDS: 4.1, 4.2, 4.3 complete]
  ├─ 4.4.1: highWaterMark [PARALLEL OK]
  ├─ 4.4.2: writableEnded/readableEnded [PARALLEL OK]
  ├─ 4.4.3: destroy() [PARALLEL OK]
  ├─ 4.4.4: Error propagation [DEPENDS: 4.4.3]
  ├─ 4.4.5: Premature end handling [DEPENDS: 4.4.4]
  ├─ 4.4.6: finished() utility [PARALLEL OK]
  └─ 4.4.7: Test [DEPENDS: ALL]
```

### Critical Path

**Longest path: 4.1.1 → 4.1.2 → 4.1.3 → 4.1.6 → 4.2.1 → 4.2.2 → 4.2.3 → 4.2.6 → 4.3.1 → 4.3.2 → 4.3.3 → 4.3.6 → 4.4.7**
- Estimated: ~18 tasks on critical path
- No parallelization possible between groups (sequential dependencies)

### Parallel Opportunities

**Within Task 4.4 (Advanced Features)**:
- Tasks 4.4.1, 4.4.2, 4.4.3, 4.4.6 can execute in parallel
- Tasks 4.4.4, 4.4.5 depend on 4.4.3

## Implementation Strategy

### Phase 1: IncomingMessage as Readable (Tasks 4.1.1-4.1.6)

#### Approach
1. **Inherit from stream.Readable** (4.1.1)
   - Store JSStreamData* in JSHttpRequest
   - Initialize in http_incoming.c constructor
   - Add Readable methods to prototype

2. **Emit 'data' events** (4.1.2)
   - Modify client_on_body() in http_parser.c
   - Emit 'data' with Buffer chunks
   - Respect paused state

3. **Emit 'end' event** (4.1.3)
   - Modify client_on_message_complete()
   - Emit 'end' after all data
   - Set readableEnded = true

4. **pause()/resume()** (4.1.4)
   - Add pause()/resume() methods
   - Control socket reading via uv_read_stop/start
   - Integrate with back-pressure

5. **pipe() support** (4.1.5)
   - Leverage stream.Readable.pipe()
   - Test req.pipe(res) pattern
   - Test res.pipe(fs.createWriteStream())

6. **Test** (4.1.6)
   - Unit tests for each feature
   - Integration tests with server

### Phase 2: ServerResponse as Writable (Tasks 4.2.1-4.2.6)

#### Approach
1. **Inherit from stream.Writable** (4.2.1)
   - Store JSStreamData* in JSHttpResponse
   - Initialize in http_response.c constructor
   - Add Writable methods to prototype

2. **_write() implementation** (4.2.2)
   - Replace current write() with _write(chunk, encoding, callback)
   - Call callback on socket write complete
   - Handle back-pressure via return value

3. **_final() implementation** (4.2.3)
   - Replace current end() logic
   - Implement _final(callback)
   - Send chunked terminator
   - Call callback when done

4. **cork()/uncork()** (4.2.4)
   - Implement cork() to buffer writes
   - Implement uncork() to flush
   - Optimize multiple small writes

5. **pipe() support** (4.2.5)
   - Test fs.createReadStream().pipe(res)
   - Test req.pipe(res) proxy pattern
   - Handle errors during pipe

6. **Test** (4.2.6)
   - Unit tests for _write/_final
   - Integration tests with client
   - Performance tests for large files

### Phase 3: ClientRequest as Writable (Tasks 4.3.1-4.3.6)

#### Approach
1. **Inherit from stream.Writable** (4.3.1)
   - Store JSStreamData* in JSHTTPClientRequest
   - Initialize in http_client.c constructor
   - Add Writable methods to prototype

2. **_write() implementation** (4.3.2)
   - Replace current write() with _write(chunk, encoding, callback)
   - Send headers on first write if not sent
   - Call callback on socket write complete

3. **_final() implementation** (4.3.3)
   - Replace current end() logic
   - Implement _final(callback)
   - Wait for response after finalization

4. **Chunked encoding** (4.3.4)
   - Auto-enable for streaming requests
   - Set Transfer-Encoding: chunked
   - Format chunks correctly

5. **flushHeaders()** (4.3.5)
   - Implement flushHeaders() to send headers early
   - Useful for long-polling, SSE
   - Don't wait for body

6. **Test** (4.3.6)
   - Unit tests for request streaming
   - Integration tests with server
   - Test POST/PUT with large files

### Phase 4: Advanced Features (Tasks 4.4.1-4.4.7)

#### Approach
1. **highWaterMark** (4.4.1 - PARALLEL OK)
   - Respect options.highWaterMark
   - Default 16KB
   - Control back-pressure threshold

2. **State properties** (4.4.2 - PARALLEL OK)
   - Implement writableEnded getter
   - Implement readableEnded getter
   - Track stream lifecycle

3. **destroy()** (4.4.3 - PARALLEL OK)
   - Implement destroy() for all HTTP streams
   - Emit 'close' event
   - Clean up resources

4. **Error propagation** (4.4.4 - DEPENDS: 4.4.3)
   - Socket errors → stream 'error'
   - Stream errors → socket close
   - Proper cleanup on error

5. **Premature end** (4.4.5 - DEPENDS: 4.4.4)
   - Detect incomplete responses
   - Emit error on premature end
   - Handle connection drops

6. **finished() utility** (4.4.6 - PARALLEL OK)
   - Implement stream.finished(stream, callback)
   - Detect stream completion
   - Handle errors in callback

7. **Test** (4.4.7 - DEPENDS: ALL)
   - Comprehensive integration tests
   - Error scenario tests
   - Performance tests

## Execution Timeline

### Batch 1: IncomingMessage Foundation (1 hour)
- Execute Tasks 4.1.1-4.1.6 sequentially
- ASAN validation after 4.1.6
- Checkpoint: IncomingMessage works as Readable stream

### Batch 2: ServerResponse Integration (45 minutes)
- Execute Tasks 4.2.1-4.2.6 sequentially
- ASAN validation after 4.2.6
- Checkpoint: ServerResponse works as Writable stream

### Batch 3: ClientRequest Integration (45 minutes)
- Execute Tasks 4.3.1-4.3.6 sequentially
- ASAN validation after 4.3.6
- Checkpoint: ClientRequest works as Writable stream

### Batch 4: Advanced Features (30 minutes)
- Execute Tasks 4.4.1, 4.4.2, 4.4.3, 4.4.6 in parallel
- Execute Tasks 4.4.4, 4.4.5 sequentially after 4.4.3
- Execute Task 4.4.7
- ASAN validation
- Checkpoint: All Phase 4 complete

## Key Technical Decisions

### Stream Integration Pattern

**Option 1: Composition (CHOSEN)**
```c
typedef struct {
  JSContext* ctx;
  JSValue request_obj;
  JSStreamData* stream;  // Owned stream.Readable instance
  // ... existing fields
} JSHttpRequest;
```

Advantages:
- Clean separation of concerns
- Reuses existing stream infrastructure
- Easy to test stream behavior independently

**Option 2: Inheritance**
```c
typedef struct {
  JSStreamData base;  // Embed stream data
  // ... HTTP-specific fields
} JSHttpRequest;
```

Disadvantages:
- Tighter coupling
- More complex memory management
- Harder to maintain

### Data Flow Architecture

**Server-side (IncomingMessage)**:
```
libuv socket → llhttp parser → on_body() → emit('data', chunk) → user
                                                               ↓
                                                          pause()/resume()
                                                               ↓
                                                     uv_read_stop/start
```

**Server-side (ServerResponse)**:
```
user → write(chunk) → _write(chunk, cb) → socket.write() → libuv
                                      ↓
                                   callback() → emit('drain')
```

**Client-side (ClientRequest)**:
```
user → write(chunk) → _write(chunk, cb) → send_headers() → socket.write()
                                      ↓                          ↓
                                   callback()              send body chunk
```

**Client-side (IncomingMessage)**:
```
socket → llhttp parser → client_on_body() → emit('data', chunk) → user
                                       ↓
                              client_on_message_complete()
                                       ↓
                                emit('end')
```

## Testing Strategy

### Unit Tests (After Each Task Group)
- Task 4.1.6: IncomingMessage streaming
- Task 4.2.6: ServerResponse streaming
- Task 4.3.6: ClientRequest streaming
- Task 4.4.7: Advanced features

### Integration Tests (After Phase 4 Complete)
- req.pipe(res) proxy pattern
- fs.createReadStream().pipe(res) file serving
- req.pipe(fs.createWriteStream()) file upload
- Error propagation scenarios
- Memory leak tests (ASAN)

### Performance Tests
- Large file streaming (100MB+)
- Many small chunks (back-pressure)
- Concurrent requests

## Success Criteria

- [ ] All 25 Phase 4 tasks completed
- [ ] IncomingMessage emits 'data' and 'end' events
- [ ] ServerResponse supports write() callbacks
- [ ] ClientRequest streams request bodies
- [ ] pipe() works for req.pipe(res) pattern
- [ ] All 165/165 existing tests pass
- [ ] No new WPT regressions
- [ ] ASAN clean (zero memory leaks)
- [ ] Plan document updated

## Risk Mitigation

### High Risk: Stream Lifecycle Management
**Risk**: Complex interactions between HTTP and stream lifecycles
**Mitigation**:
- Careful state tracking (ended, finished, destroyed)
- Comprehensive error handling
- Extensive testing

### Medium Risk: Back-pressure Handling
**Risk**: Improper back-pressure causes memory issues
**Mitigation**:
- Respect highWaterMark in all paths
- Test with large data transfers
- Monitor memory usage with ASAN

### Low Risk: API Compatibility
**Risk**: Breaking existing HTTP functionality
**Mitigation**:
- Run full test suite after each batch
- Ensure backward compatibility
- Document any API changes

## Next Steps

1. Create todo list with all 25 tasks
2. Execute Batch 1 (IncomingMessage)
3. Validate with tests and ASAN
4. Execute Batch 2 (ServerResponse)
5. Validate with tests and ASAN
6. Execute Batch 3 (ClientRequest)
7. Validate with tests and ASAN
8. Execute Batch 4 (Advanced Features)
9. Final validation (tests + ASAN + WPT)
10. Update main plan document

---

**Estimated Total Time**: 2-3 hours
**Critical Path**: IncomingMessage → ServerResponse → ClientRequest → Advanced
**Parallel Opportunities**: Limited (only within Task 4.4)
**ASAN Checkpoints**: After each batch (4 total)
