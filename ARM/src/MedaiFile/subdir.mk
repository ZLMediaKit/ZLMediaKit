################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/MedaiFile/HLSMaker.cpp \
../src/MedaiFile/MediaReader.cpp \
../src/MedaiFile/MediaRecorder.cpp \
../src/MedaiFile/Mp4Maker.cpp \
../src/MedaiFile/TSMaker.cpp 

OBJS += \
./src/MedaiFile/HLSMaker.o \
./src/MedaiFile/MediaReader.o \
./src/MedaiFile/MediaRecorder.o \
./src/MedaiFile/Mp4Maker.o \
./src/MedaiFile/TSMaker.o 

CPP_DEPS += \
./src/MedaiFile/HLSMaker.d \
./src/MedaiFile/MediaReader.d \
./src/MedaiFile/MediaRecorder.d \
./src/MedaiFile/Mp4Maker.d \
./src/MedaiFile/TSMaker.d 


# Each subdirectory must supply rules for building sources it contributes
src/MedaiFile/%.o: ../src/MedaiFile/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	arm-linux-gnueabi-g++ -std=c++1y -I/home/xzl/soft -I"/Users/xzl/git/ZLMediaKit/src" -I../../ZLToolKit/src -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


