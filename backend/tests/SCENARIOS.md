# Test Scenarios

The standalone mini-programs in `tests/scenarios` cover the main stages of the backend workflow:

1. `register_flow` - create a new user account and validate duplicate registration handling.
2. `login_flow` - authenticate an existing user and verify token/session creation.
3. `public_key_flow` - fetch the public key for a known user and check the not-found branch.
4. `message_send_flow` - send an encrypted message and validate storage and authorization rules.
5. `conversation_poll_flow` - poll a conversation and ensure only the requested dialog is returned.
