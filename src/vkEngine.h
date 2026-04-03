#pragma once

void initialise(int argc, char** argv);
void initDevice(int argc, char** argv);
void initSwapchain();
void initBufferAccess();
void initSync();
void initCommandPool();
void initModel();
void initTextures();
void initDepthBuffer();
void initDescriptors();
void initShader();
void initGraphicsPipeline();
void run();
void updateSwapchain();
void cleanup();
