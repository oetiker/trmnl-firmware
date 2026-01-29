#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <device_identity.h>

/**
 * Pre-computed Ed25519 authentication headers.
 *
 * Computed before opening the main HTTP connection (signing needs a server
 * time fetch, and nesting HTTP connections is not supported).
 */
struct Ed25519Headers {
  bool present;
  String publicKey;
  String signature;
  String timestamp;
};

/**
 * Compute the Ed25519 auth headers for a request.
 *
 * Returns {.present = false} when Ed25519 auth is not enabled, the identity is
 * unavailable, or any step (server time fetch, signing) fails.
 *
 * @param authMode Configured auth mode ("api_key" or "ed25519")
 * @param identity Device identity (may be nullptr)
 * @param baseUrl  Base URL of the API server, used to fetch server time
 */
Ed25519Headers computeEd25519Headers(const String &authMode,
                                     const DeviceIdentity *identity,
                                     const String &baseUrl);

/**
 * Add previously computed Ed25519 headers to a request. No-op if not present.
 */
void addEd25519Headers(HTTPClient &https, const Ed25519Headers &auth);
