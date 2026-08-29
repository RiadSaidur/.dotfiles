#!/bin/bash

# Check if DND is currently enabled
if makoctl mode | grep -q "do-not-disturb"; then
    # Disable DND
    makoctl mode -r do-not-disturb
    echo ""  # Change icon to notifications enabled
else
    # Enable DND
    makoctl mode -a do-not-disturb
    echo "dnd"  # Change icon to DND enabled
fi
