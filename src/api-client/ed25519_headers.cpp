#include <api-client/ed25519_headers.h>
#include <api-client/time.h>
#include <auth_signature.h>
#include <config.h>
#include <trmnl_log.h>

Ed25519Headers computeEd25519Headers(const String &authMode,
                                     const DeviceIdentity *identity,
                                     const String &baseUrl)
{
  if (authMode != PREFERENCES_AUTH_MODE_ED25519 ||
      identity == nullptr ||
      !identity->initialized)
  {
    return {.present = false};
  }

  Log_info("Pre-computing Ed25519 authentication");
  auto timeResult = fetchServerTime(baseUrl);
  if (!timeResult.success)
  {
    Log_error("Failed to fetch server time: %s", timeResult.error.c_str());
    return {.present = false};
  }

  auto sig = computeAuthSignature(*identity, timeResult.timestamp_ms);
  if (!sig.valid)
  {
    Log_error("Failed to compute Ed25519 signature");
    return {.present = false};
  }

  return {
    .present = true,
    .publicKey = publicKeyToHex(*identity),
    .signature = signatureToHex(sig),
    .timestamp = timestampToString(sig.timestamp_ms),
  };
}

void addEd25519Headers(HTTPClient &https, const Ed25519Headers &auth)
{
  if (auth.present)
  {
    https.addHeader("X-Public-Key", auth.publicKey);
    https.addHeader("X-Signature", auth.signature);
    https.addHeader("X-Timestamp", auth.timestamp);
    Log_info("Added Ed25519 auth headers");
  }
}
