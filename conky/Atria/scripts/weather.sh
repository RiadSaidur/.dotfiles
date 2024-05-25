#!/bin/bash

# Your OpenWeather API key
API_KEY="b33d3c6dbe5a3a4cde65619771f5bfdb"

# The city for which you want to get the weather
CITY="dhaka"

# Get the weather data in JSON format
weather_data=$(curl -s "http://api.openweathermap.org/data/2.5/weather?q=${CITY}&appid=${API_KEY}&units=metric")

# Parse the JSON to get the desired information
temperature=$(echo $weather_data | grep -oP '"temp":\K[^,]*')
rounded_temperature=$(printf "%.0f" $temperature)
description=$(echo $weather_data | grep -oP '"description":"\K[^"]*')

# Print the weather information
echo "${rounded_temperature=}°C      ${description}"
