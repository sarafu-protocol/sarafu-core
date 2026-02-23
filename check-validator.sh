#!/bin/bash
# Sarafu Validator Status Check Script

echo "========================================="
echo "  Sarafu Validator Status Check"
echo "========================================="
echo ""

# Check if node is running
if pgrep -f "sarafu-node.*--validator" > /dev/null; then
    PID=$(pgrep -f "sarafu-node.*--validator")
    echo "✓ Validator node is RUNNING (PID: $PID)"
    
    # Get process info
    echo ""
    echo "Process Information:"
    ps -p $PID -o pid,ppid,%cpu,%mem,etime,command | tail -1
    
    # Check network ports
    echo ""
    echo "Network Ports:"
    lsof -i -P -n | grep sarafu-node | grep LISTEN || echo "  No listening ports found"
    
    # Check data directory
    echo ""
    echo "Data Directory:"
    if [ -d "./data/local" ]; then
        echo "  Location: ./data/local"
        echo "  Size: $(du -sh ./data/local 2>/dev/null | cut -f1)"
        echo "  Files: $(find ./data/local -type f 2>/dev/null | wc -l | tr -d ' ')"
    else
        echo "  Not created yet"
    fi
    
    # Validator info
    echo ""
    echo "Validator Configuration:"
    echo "  Chain ID: 1337 (local)"
    echo "  Consensus Key: ./docker/keys/validator-1/consensus_key.bin"
    echo "  Withdrawal Key: ./docker/keys/validator-1/withdrawal_key.bin"
    
else
    echo "✗ Validator node is NOT RUNNING"
    echo ""
    echo "To start the validator, run:"
    echo "  ./build/sarafu-node --config config.local.toml --validator \\"
    echo "    --consensus-key ./docker/keys/validator-1/consensus_key.bin \\"
    echo "    --withdrawal-key ./docker/keys/validator-1/withdrawal_key.bin \\"
    echo "    --log-level INFO &"
fi

echo ""
echo "========================================="
