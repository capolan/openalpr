Calibrating your camera improves detection accuracy in cases where vehicle plates are captured at a steep angle.  For example, the plate below is captured at 40+ degree horizontal angle, and will normally not be recognized as a license plate.  

![](http://doc.openalpr.com/_images/configuration_calibration_before.jpg)

The camera calibration helps in cases where the camera is capturing from a fixed position, and all plates generally are seen at the same angle.  This feature is best for short-angle cameras or cameras capturing at a close distance.


Use the tool named "openalpr-utils-calibrate" to calibrate your camera.  The utility needs a single example image to begin.  This image should be taken from the camera you wish to calibrate and should have a license plate that represents the typical distance/angle of plate captured on this camera.

    ./openalpr-utils-calibrate camera_image.jpg

There are sliders at the top of the screen that control the skew.  Adjust the sliders until the plate looks like it is seen from straight-on with no angle.  Left-click and drag to draw a box (with the correct aspect ratio) to test if the plate is the correct size.

You can also right-click and drag to move the image within the frame.  Because we are distorting the original image, the frame will be clipped.  If frames are unlikely to be seen in certain areas (on the ceiling for example) you may want to adjust the plate image to ensure that those areas will be cropped.

![](http://doc.openalpr.com/_images/configuration_calibration_tool.jpg)

Once you're satisfied with the parameters you've chosen, press the 'o' key.  This produces a line of configuration in the terminal that can be copied directly into openalpr.conf as the "prewarp" config.  This will apply the camera calibration settings against each image before it is used for plate recognition.

Now test the new settings on a few images from this camera to make sure that the new calibration improves accuracy.

    ./alpr camera_image.jpg

![](http://doc.openalpr.com/_images/configuration_calibration_after.jpg)

Notice that the license plate is now correctly detected.  You can view the calibrated image results by enabling the "prewarp" debug option in the openalpr.conf file.  Test the accuracy on many different images from the camera before accepting the configuration.  Other calibration angles could produce superior results.