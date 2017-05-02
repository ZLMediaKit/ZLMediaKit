################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/config.cpp 

OBJS += \
./src/config.o 

CPP_DEPS += \
./src/config.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -std=c++1y -DENABLE_RTSP2RTMP -DENABLE_RTMP2RTSP -DENABLE_HLS -DENABLE_MP4V2 -DENABLE_FAAC -DENABLE_X264 -I"/home/xzl/git/ZLMediaKit/src" -I../../ZLToolKit/src -O3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


