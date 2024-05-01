The pattern matching feature runs the topN results against a Regular expression matcher to find results that match common license plate patterns.  The regex patterns are customizable and can be found in runtime_data/postprocess/*.patterns

For example, using a pattern against this Czechoslovakian plate results in only one possible match (which is the correct one).  

![cz_4s50233](https://cloud.githubusercontent.com/assets/508260/7060031/b637f8a8-de47-11e4-9cf8-7c66d3c4d269.jpg)

The cz patterns are:
 - cz    #@#####
 - cz    #@@####


Results for this plate, notice the pattern matches 4S50233:

    [mhill@mhill-linux tmp]$ alpr -c eu -p cz cz_4s50233.jpg -n 40
    Config file location provided via default location
    plate0: 40 results
        - 4S5O233     confidence: 90.947      pattern_match: 0
        - 4S5O23      confidence: 87.8683     pattern_match: 0
        - 4S5O23      confidence: 85.1644     pattern_match: 0
        - 4S5O23S     confidence: 84.5445     pattern_match: 0
        - 4S5O23B     confidence: 83.7395     pattern_match: 0
        - 4S5O2S3     confidence: 83.3698     pattern_match: 0
        - 4S5O23G     confidence: 83.1375     pattern_match: 0
        - 4S50233     confidence: 83.0457     pattern_match: 1
        - 4S5O2B3     confidence: 82.5635     pattern_match: 0
        - 4S5O2       confidence: 82.0857     pattern_match: 0
        - 4S5O2G3     confidence: 81.5684     pattern_match: 0
        - 4S5O2J3     confidence: 81.0409     pattern_match: 0
        - 4S5O2S      confidence: 80.2911     pattern_match: 0
        ... more results that do not match ...

You can utilize this from the library code by calling "setDefaultRegion(string region)" with the name of the pattern you wish to use: