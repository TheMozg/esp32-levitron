#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>

#include <memory>
#include <atomic>
#include <array>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace fk {
    WebServer m_server;

    void loopTask(void* pvParameters)
    {
        for (;;) {
            m_server.handleClient();
            delay(1);
        }
    }

    void handleRoot() {
        String html = "<html>\
        <head>\
        <script src=\"//cdnjs.cloudflare.com/ajax/libs/dygraph/2.1.0/dygraph.min.js\"></script>\
        <link rel=\"stylesheet\" href=\"//cdnjs.cloudflare.com/ajax/libs/dygraph/2.1.0/dygraph.min.css\" />\
        </head>\
        <body>\
        <h4>Graph</h4> \
        <div id=\"graphdiv\" style=\"width:1200px; height:600px;\"></div>\
        <script type=\"text/javascript\">\
        g = new Dygraph(document.getElementById(\"graphdiv\"),\"adc.csv\",{});\
        </script>\
        </body>\
        </html>";
        m_server.send(200, "text/html", html);
    }

    void startServer(const char * ssid, const char * password) {
        Serial.begin(115200);

        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);
        Serial.println("");

        // Wait for connection
        while (WiFi.status() != WL_CONNECTED) {
            delay(100);
            Serial.print(".");
        }

        Serial.println("");
        Serial.print("Connected to ");
        Serial.println(ssid);
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        if (MDNS.begin("esp32")) {
            Serial.println("MDNS responder started");
        }

        m_server.on("/", handleRoot);
        m_server.begin();
        Serial.println("HTTP server started");

        xTaskCreatePinnedToCore(
            loopTask,           /* Task function. */
            "TaskWebServer",    /* name of task. */
            10000,              /* Stack size of task */
            NULL,               /* parameter of the task */
            1,                  /* priority of the task */
            NULL,               /* Task handle to keep track of created task */
            0);                 /* pin task to core 0 */
    }

    #define WBUF 100
    template<typename T, size_t W, size_t S>
    class Graph {
    public:
        Graph(std::array<const char *, W> h):
            nextw(0),
            headers(h),
            size(S),
            sizei(S+WBUF)
            {}
        const uint16_t size;
        const uint16_t sizei;
        const std::array<const char *, W> headers;
        std::array<std::array<T, W>, S+WBUF> buf;
        std::atomic<uint16_t> nextw;
        uint16_t readhead;
        void push(const std::array<T, W>& data) {
            buf[nextw] = data;
            nextw = (nextw + 1) % sizei;
        }
        void start_read() {
            readhead = (nextw + WBUF) % sizei;
        }
        std::array<T, W> get(uint16_t i) {
            i = (readhead + i) % sizei;
            return buf[i];
        }
    };

    #define CHUNK_SIZE 500
    template<typename T, size_t W, size_t S>
    void addGraph(Graph<T,W,S>& graph) {
        m_server.on("/adc.csv", [&graph]() {
            auto st = millis();
            String csv = "N";
            for(const auto& h: graph.headers) {
                csv.concat(',');
                csv.concat(h);
            };
            csv.concat('\n');
            m_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
            m_server.send(200, "text/comma-separated-values", csv);
            size_t full_chunks = graph.size / CHUNK_SIZE;
            size_t last_chunk_length = graph.size % CHUNK_SIZE;
            graph.start_read();
            uint16_t sa = graph.nextw;
            for (size_t chunk = 0; chunk < full_chunks; chunk++)
            {
                csv = "";
                for (int i = chunk * CHUNK_SIZE; i < (chunk + 1) * CHUNK_SIZE; i++)
                {
                    csv.concat(i);
                    for(const auto& s: graph.get(i)) {
                        csv.concat(',');
                        csv.concat(s);
                    }
                    csv.concat('\n');
                }
                m_server.sendContent(csv);
            }
            csv = "";
            for (int i = full_chunks * CHUNK_SIZE; i < full_chunks * CHUNK_SIZE + last_chunk_length; i++)
            {
                csv.concat(i);
                for(const auto& s: graph.get(i)) {
                    csv.concat(',');
                    csv.concat(s);
                }
                csv.concat('\n');
            }
            m_server.sendContent(csv);
            m_server.sendContent("");
            Serial.print("Served csv file in ");
            Serial.print(millis() - st);
            Serial.print("ms (");
            Serial.print(graph.nextw - sa);
            Serial.println(" writes).");
        });
    };

}
