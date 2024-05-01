alprd
=====
The OpenALPR daemon allows you to monitor a camera stream for license plate numbers in the background.  Alprd runs as a daemon process on Linux.  The plate numbers can be streamed to another server (via HTTP posts) or can be consumed programmatically via a beanstalkd queue.

Architecture
-------------
Alprd operates as follows:
  1. The image stream is constantly pulled from the IP camera via MJPEG over HTTP
  2. alprd processes the stream as fast as it can looking for plate images.  The daemon automatically skips frames to stay in-sync with clock time.
  3. When one or more plates are detected, the information is written to a local beanstalkd queue (tube name is alprd) as JSON data.
  4. Optionally, alprd will also save the image as a jpeg and save it to a configurable location.
  5. Optionally, alprd also runs a separate process that drains the beanstalkd queue and uploads data to a remote HTTP server via POST.

Alprd can be used in two modes:
  1. Recognition results are streamed to an HTTP server
  2. Recognition results can be read from the beanstalkd queue

```                                                                     
    +------------------+                     +-------------+         
    |                  |  MJPEG       POST   |             |         
    |  Network Camera  | <---+      +------> | HTTP Server |         
    |                  |     |      |        |             |         
    +------------------+     |      |        +-------------+         
                             |      |                                
                             |      |                                
                             |      |                                
                     +-------+------+                                
                     |              |                                
                     | alprd server |                                
                     |              |                                
                     +---------+----+-------------+                   
                               |                  |                   
                               | Beanstalkd queue |                   
                               |                  |                   
                               +------------------+                   
```

The diagram above shows alprd being used to stream data to another HTTP server.  alprd is configured with a remote HTTP address.  As plates are identified, the server sends the JSON data to the remote HTTP server.  The beanstalkd queue and the alprd process are colocated on the same server.

```
+------------------+                                         
|                  |  MJPEG                                  
|  Network Camera  | <---+                                   
|                  |     |                                   
+------------------+     |                                   
                         |                        +----------+
                         |                        |Processing|
                         |                        +----+-----+
                 +-------+------+                      |      
                 |              |                      |      
                 | alprd server |                      |      
                 |              |                      |      
                 +---------+----+-------------+        |      
                           |                  |        |      
                           | Beanstalkd queue | <------+      
                           |                  |               
                           +------------------+               
```

The diagram above shows alprd being used without the HTTP server.  In this case, a beanstalkd consumer can be used to drain the results from the beanstalkd queue.  The beanstalkd tube name is "alprd."  Beanstalkd consumers can be written in any language, and can be colocated on the alprd server or located elsewhere.

Configuration
-------------

    [daemon]

    ; Declare each stream on a separate line
    ; each unique stream should be defined as stream = [url]
    
    ;stream = http://www.google.com/video.stream
    ;stream = http://stream2.com/stream
    
    site_id = site-name
    
    store_plates = 0
    store_plates_location = /var/www/html/plates/
    
    ; upload address is the destination to POST to
    upload_data = 0
    upload_address = http://localhost:9000/alpr/push/

alprd needs at least one "stream" defined.  This is just the URL for the mjpeg stream.  You may use multiple streams on one server -- each stream spawns a separate process that attempts to use a full CPU core.

The site-id will be stored along with the JSON plate results.  This is especially useful if you have multiple servers and need to keep track of where the results are coming from.  Additionally, each result will contain a camera ID (numbered 1 to n) based on the order of your "stream" statements in the alprd.conf file

Results
---------
The following is an example of the JSON results.  These results are initially stored in the beanstalkd queue, and then optionally sent in an HTTP post.

    {
      "version": 1,
      "uuid": "e11e0acc-6aaf-4817-9299-9e6773043b8e",
      "camera_id": 1,
      "site_id": "watchtower-hq",
      "img_width": 640,
      "img_height": 480,
      "epoch_time": 1402161050,
      "processing_time_ms": 138.669163,
      "results": [
        {
          "plate": "S11FRE",
          "confidence": 77.130661,
          "matches_template": 0,
          "region": "",
          "region_confidence": 0,
          "coordinates": [
            {
              "x": 218,
              "y": 342
            },
            {
              "x": 407,
              "y": 325
            },
            {
              "x": 407,
              "y": 413
            },
            {
              "x": 218,
              "y": 431
            }
          ],
          "candidates": [
            {
              "plate": "S11FRE",
              "confidence": 77.130661,
              "matches_template": 0
            },
            {
              "plate": "S11ERE",
              "confidence": 75.496307,
              "matches_template": 0
            },
            {
              "plate": "S11RE",
              "confidence": 75.440361,
              "matches_template": 0
            },
            {
              "plate": "S11CRE",
              "confidence": 75.340179,
              "matches_template": 0
            },
            {
              "plate": "S11FHE",
              "confidence": 75.240974,
              "matches_template": 0
            },
            {
              "plate": "S11EHE",
              "confidence": 73.606621,
              "matches_template": 0
            },
            {
              "plate": "S11HE",
              "confidence": 73.550682,
              "matches_template": 0
            },
            {
              "plate": "S11CHE",
              "confidence": 73.450493,
              "matches_template": 0
            },
            {
              "plate": "S11FBE",
              "confidence": 71.782944,
              "matches_template": 0
            },
            {
              "plate": "S11FE",
              "confidence": 71.762756,
              "matches_template": 0
            },
            {
              "plate": "S11EBE",
              "confidence": 70.148582,
              "matches_template": 0
            },
            {
              "plate": "S11EE",
              "confidence": 70.128403,
              "matches_template": 0
            },
            {
              "plate": "S11BE",
              "confidence": 70.092636,
              "matches_template": 0
            },
            {
              "plate": "S11E",
              "confidence": 70.072449,
              "matches_template": 0
            },
            {
              "plate": "S11CBE",
              "confidence": 69.992455,
              "matches_template": 0
            },
            {
              "plate": "S11CE",
              "confidence": 69.972267,
              "matches_template": 0
            },
            {
              "plate": "S11FME",
              "confidence": 69.581451,
              "matches_template": 0
            },
            {
              "plate": "S11F8E",
              "confidence": 68.718605,
              "matches_template": 0
            },
            {
              "plate": "S11EME",
              "confidence": 67.947098,
              "matches_template": 0
            },
            {
              "plate": "S11ME",
              "confidence": 67.891144,
              "matches_template": 0
            },
            {
              "plate": "S11CME",
              "confidence": 67.790962,
              "matches_template": 0
            },
            {
              "plate": "S11F9E",
              "confidence": 67.744087,
              "matches_template": 0
            },
            {
              "plate": "S11E8E",
              "confidence": 67.084251,
              "matches_template": 0
            },
            {
              "plate": "S118E",
              "confidence": 67.028305,
              "matches_template": 0
            },
            {
              "plate": "S11C8E",
              "confidence": 66.928123,
              "matches_template": 0
            }
          ]
        },
        {
          "plate": "EJLESSIE",
          "epoch_time": 1402158050,
          "confidence": 78.167984,
          "matches_template": 0,
          "region": "",
          "region_confidence": 0,
          "processing_time_ms": 51.650604,
          "coordinates": [
            {
              "x": 226,
              "y": 369
            },
            {
              "x": 348,
              "y": 348
            },
            {
              "x": 355,
              "y": 406
            },
            {
              "x": 231,
              "y": 429
            }
          ],
          "candidates": [
            {
              "plate": "EJLESSIE",
              "confidence": 78.167984,
              "matches_template": 0
            },
            {
              "plate": "VI5A",
              "confidence": 58.167984,
              "matches_template": 0
            },
            {
              "plate": "EDLESSIE",
              "confidence": 77.61319,
              "matches_template": 0
            }
          ]
        }
      ]
    }
    
                                                                     