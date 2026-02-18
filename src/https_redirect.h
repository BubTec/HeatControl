#pragma once

namespace HeatControl {

// Starts a small HTTPS listener on port 443 that redirects to the HTTP UI.
void startHttpsRedirectServer();

}  // namespace HeatControl

