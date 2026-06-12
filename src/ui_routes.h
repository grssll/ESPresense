/*
 * Web UI Routes
 *
 * Compressed Size Summary:
 * ui_app_immutable_assets_css: 25,148 bytes
 * ui_html: 4,510 bytes
 * ui_app_immutable_entry_js: 68,602 bytes
 * ui_app_immutable_nodes_js: 590 bytes
 * ui_svg: 456 bytes
 * Total: 99,306 bytes
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
    server->on("/app/immutable/entry/app.Bufsh-Me.js", HTTP_GET, serveAppImmutableEntryAppBufshMeJs);
    server->on("/app/immutable/entry/start.D3XyfK_x.js", HTTP_GET, serveAppImmutableEntryStartD3XyfKXJs);
    server->on("/app/immutable/nodes/0.Dmgj0Kj7.js", HTTP_GET, serveAppImmutableNodes_0Dmgj0Kj7Js);
    server->on("/app/immutable/nodes/1.DGLse1a3.js", HTTP_GET, serveAppImmutableNodes_1DgLse1a3Js);
    server->on("/app/immutable/nodes/2.DRW-14oJ.js", HTTP_GET, serveAppImmutableNodes_2Drw_14oJJs);
    server->on("/app/immutable/nodes/3.DMB2udx_.js", HTTP_GET, serveAppImmutableNodes_3Dmb2udxJs);
    server->on("/app/immutable/nodes/4.nlZcxG2Q.js", HTTP_GET, serveAppImmutableNodes_4NlZcxG2QJs);
    server->on("/app/immutable/nodes/5.1iRmx0Ji.js", HTTP_GET, serveAppImmutableNodes_5_1iRmx0JiJs);
    server->on("/app/immutable/nodes/6.eU-ZIa3_.js", HTTP_GET, serveAppImmutableNodes_6EUZIa3Js);
    server->on("/app/immutable/nodes/7.BeWyhWK8.js", HTTP_GET, serveAppImmutableNodes_7BeWyhWk8Js);
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
