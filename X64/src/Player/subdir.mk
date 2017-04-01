################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/Player/MediaPlayer.cpp \
../src/Player/Player.cpp \
../src/Player/PlayerBase.cpp 

OBJS += \
./src/Player/MediaPlayer.o \
./src/Player/Player.o \
./src/Player/PlayerBase.o 

CPP_DEPS += \
./src/Player/MediaPlayer.d \
./src/Player/Player.d \
./src/Player/PlayerBase.d 


# Each subdirectory must supply rules for building sources it contributes
src/Player/%.o: ../src/Player/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -std=c++1y -DENABLE_FAAC -DENABLE_RTSP2RTMP -DENABLE_RTMP2RTSP -DENABLE_MEDIAFILE -DENABLE_X264 -I"/Users/xzl/git/ZLMediaKit/src" -I../../ZLToolKit/src -O3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


