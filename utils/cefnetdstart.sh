#!/bin/bash
#
# Copyright (c) 2016-2023, National Institute of Information and Communications
# Technology (NICT). All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the NICT nor the names of its contributors may be
#    used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE NICT AND CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE NICT OR CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# Color definitions for enhanced output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Function to print colored messages
print_info() {
    echo -e "${CYAN}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check the number of parameters 
if [ $# -gt 4 ]; then
    print_error 'usage : cefnetdstart [-d config_file_dir] [-p port_num]'
    exit 1
fi

# Check if cefnetd is already running
print_info "Checking if cefnetd is already running..."
if pgrep -f "cefnetd" > /dev/null; then
    print_warning "cefnetd appears to be already running:"
    ps aux | grep cefnetd | grep -v grep | while read line; do
        echo "  $line"
    done
    print_info "Continuing with startup anyway..."
fi

# Display startup information
print_info "Starting CEFORE daemon (cefnetd)..."
print_info "Arguments: $@"
print_info "User: $(whoami)"
print_info "Timestamp: $(date '+%Y-%m-%d %H:%M:%S')"
print_info "Executing: cefnetd $@ &"

# Start cefnetd in background
cefnetd $@ &
CEFNETD_PID=$!

# Wait a moment to check if the process started successfully
sleep 2

# Check if cefnetd is running
if kill -0 $CEFNETD_PID 2>/dev/null; then
    print_success "=== CEFORE DAEMON SUCCESSFULLY STARTED ==="
    print_info "Process ID: $CEFNETD_PID"
    
    # Extract port information if specified
    PORT_ARG=""
    for arg in "$@"; do
        if [[ $arg =~ ^[0-9]+$ ]]; then
            PORT_ARG="$arg"
            break
        fi
    done
    
    if [ -n "$PORT_ARG" ]; then
        print_info "Port: $PORT_ARG"
    else
        print_info "Port: 9896 (default if not specified)"
    fi
    
    # Extract config directory if specified
    CONFIG_DIR=""
    prev_arg=""
    for arg in "$@"; do
        if [ "$prev_arg" = "-d" ]; then
            CONFIG_DIR="$arg"
            break
        fi
        prev_arg="$arg"
    done
    
    if [ -n "$CONFIG_DIR" ]; then
        print_info "Config Directory: $CONFIG_DIR"
    else
        print_info "Config Directory: /usr/local/cefore (default)"
    fi
    
    echo ""
    print_info "=== CEFORE STARTUP INFORMATION ==="
    print_info "• Use 'cefnetdstop' to stop the daemon"
    print_info "• Use 'cefstatus' to check daemon status"
    print_info "• Check logs for detailed startup information"
    print_info "• PID file location: /tmp/cefnetd.pid (if configured)"
    echo ""
    
else
    print_error "Failed to start cefnetd"
    print_error "Check the following:"
    print_error "• Ensure cefnetd binary is in PATH"
    print_error "• Check configuration files"
    print_error "• Verify network port availability"
    print_error "• Check system logs for detailed error information"
    exit 1
fi
