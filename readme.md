# Vulkaning
Simple Vulkan Application based off the work at https://howtovulkan.com/ (Huge thanks to Sascha Willems!)

This is just a dump of the work I did trying to understand the basics of Vulkan. I make a few edits compared to the source code contained there however. The main changes are:
* I split the code into many functions and put it into a separate header and source file.
* The program renders a single model instead of 3 copies of the same thing.
* Comments everywhere to explain how Vulkan things work (but you should read the guide at https://howtovulkan.com/ for the most accurate information).
* Red background (very important)

This should hopefully just compile for anyone who tries to on Linux, but I'm uncertain for Windows or MacOS. CMake hurts my brain.