/*
 * Web UI Routes
 *
 * Compressed Size Summary:
 * ui_app_immutable_assets_css: 25,148 bytes
 * ui_html: 4,527 bytes
 * ui_app_immutable_entry_js: 68,911 bytes
 * ui_app_immutable_nodes_js: 590 bytes
 * ui_svg: 456 bytes
 * Total: 99,632 bytes
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include "ui_app_immutable_assets_css.h"
#include "ui_html.h"
#include "ui_app_immutable_entry_js.h"
#include "ui_app_immutable_nodes_js.h"
#include "ui_svg.h"

inline void setupRoutes(AsyncWebServer* server) {
    server->on("/app/immutable/assets/internal.DJlzcx4I.css", HTTP_GET, serveAppImmutableAssetsInternalDJlzcx4ICss);
    server->on("/app/immutable/assets/start.DJlzcx4I.css", HTTP_GET, serveAppImmutableAssetsStartDJlzcx4ICss);
    server->on("/app/immutable/entry/app.Bkcl5MAJ.js", HTTP_GET, serveAppImmutableEntryAppBkcl5MajJs);
    server->on("/app/immutable/entry/start.kMj_rXVQ.js", HTTP_GET, serveAppImmutableEntryStartKMjRXvqJs);
    server->on("/app/immutable/nodes/0.DOhCfzt6.js", HTTP_GET, serveAppImmutableNodes_0DOhCfzt6Js);
    server->on("/app/immutable/nodes/1.Cvc5-5z4.js", HTTP_GET, serveAppImmutableNodes_1Cvc5_5z4Js);
    server->on("/app/immutable/nodes/2.CJW3kNSW.js", HTTP_GET, serveAppImmutableNodes_2Cjw3kNswJs);
    server->on("/app/immutable/nodes/3.BG40AzGl.js", HTTP_GET, serveAppImmutableNodes_3Bg40AzGlJs);
    server->on("/app/immutable/nodes/4.DBHfQru7.js", HTTP_GET, serveAppImmutableNodes_4DbHfQru7Js);
    server->on("/app/immutable/nodes/5.DNbC4zc_.js", HTTP_GET, serveAppImmutableNodes_5DNbC4zcJs);
    server->on("/app/immutable/nodes/6.BQ1lMdVd.js", HTTP_GET, serveAppImmutableNodes_6Bq1lMdVdJs);
    server->on("/app/immutable/nodes/7.DMc3D55h.js", HTTP_GET, serveAppImmutableNodes_7DMc3D55hJs);
    server->on("/favicon.svg", HTTP_GET, serveFaviconSvg);
    // HTML routes
    server->on("/devices", HTTP_GET, serveDevicesHtml);
    server->on("/devices.html", HTTP_GET, serveDevicesHtml);
    server->on("/fingerprints", HTTP_GET, serveFingerprintsHtml);
    server->on("/fingerprints.html", HTTP_GET, serveFingerprintsHtml);
    server->on("/hardware", HTTP_GET, serveHardwareHtml);
    server->on("/hardware.html", HTTP_GET, serveHardwareHtml);
    server->on("/", HTTP_GET, serveIndexHtml);
    server->on("/network", HTTP_GET, serveNetworkHtml);
    server->on("/network.html", HTTP_GET, serveNetworkHtml);
    server->on("/settings", HTTP_GET, serveSettingsHtml);
    server->on("/settings.html", HTTP_GET, serveSettingsHtml);
}
