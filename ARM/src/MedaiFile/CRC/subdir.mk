################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/MedaiFile/CRC/crc32.cpp 

OBJS += \
./src/MedaiFile/CRC/crc32.o 

CPP_DEPS += \
./src/MedaiFile/CRC/crc32.d 


# Each subdirectory must supply rules for building sources it contributes
src/MedaiFile/CRC/%.o: ../src/MedaiFile/CRC/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	arm-linux-gnueabi-g++ -std=c++1y -I/home/xzl/soft -I"/Users/xzl/git/ZLMediaKit/src" -I../../ZLToolKit/src -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


