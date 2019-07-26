#!/bin/bash

# Recreate config file
rm -rf  /usr/src/ZLMediaKitUI/ui/env-config.js
touch  /usr/src/ZLMediaKitUI/ui/env-config.js

# Add assignment
echo "window._env_ = {" >> /usr/src/ZLMediaKitUI/ui/env-config.js

echo "  REACT_APP_API_HOST: \"${REACT_APP_API_HOST}\"" >>  /usr/src/ZLMediaKitUI/ui/env-config.js

echo "}" >>  /usr/src/ZLMediaKitUI/ui/env-config.js