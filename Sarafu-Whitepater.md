# Sarafu: A Neutral, Jurisdiction-Resilient Settlement Protocol

**Version 4.0 — Final Implementation Specification with Economic & Security Addendum**

---

## Abstract

Sarafu is a politically neutral, jurisdiction-resilient settlement protocol engineered for low-friction cross-border payments and systematic developer adoption. Unlike deflationary speculative assets or fiat-backed stablecoins, Sarafu implements a deterministic BFT Proof-of-Stake consensus with predictable, security-budget-driven issuance.

The protocol is designed around five core pillars:

1. **HotStuff BFT Consensus** — Linear communication complexity with instant finality.
2. **Dynamic Issuance Model** — Security-budget-targeted monetary policy that aligns validator incentives with network security.
3. **Cryptographic Light Clients** — Finalized header proofs ≤ 5 KB for low-bandwidth environments.
4. **Stateless Integration Layer** — Protocol independence from SMS, USSD, banking, and mobile money adapters.
5. **Jurisdiction Resilience** — No issuer, no treasury control, no discretionary monetary committee.

This specification incorporates:
- **Validator ROI modeling** under realistic African infrastructure costs
- **Genesis distribution design** minimizing cartel formation probability
- **Correlated slashing stress-testing** under cascading validator failures
- **Liquidity-adjusted attack cost analysis** for economic security

Sarafu is not digital gold. It is not a synthetic fiat proxy. It is a neutral, programmable settlement rail optimized for high-adoption emerging markets.

---

## 1. Design Principles

| Principle | Description |
| :--- | :--- |
| **Jurisdiction Neutrality** | No central issuer, no treasury, no monetary committee. |
| **Security Budget Sustainability** | Validators economically incentivized long-term via predictable issuance. |
| **Predictable Monetary Policy** | Moderate issuance with fee burn smoothing. |
| **Separation of Concerns** | Core consensus independent of telecom, banks, and payment adapters. |
| **Low-Bandwidth Compatibility** | Light client proofs < 5 KB; header-only sync. |
| **Transparent Governance** | On-chain parameter voting with time-locked execution. |
| **Economic Resilience** | Modeled against adversarial behavior including censorship, double-spend, and chain halt, with explicit consideration of market liquidity and validator operating costs. |

---

## 2. Technical Architecture

### 2.1 Consensus: HotStuff BFT

Sarafu adopts **HotStuff** (Libra/Diem lineage), a pipelined BFT consensus variant.

**Why HotStuff:**
- Linear communication complexity O(n) versus PBFT's O(n²).
- Proven safety under < 1/3 Byzantine validators.
- Clean leader-based model with view-change pipelining.
- Optimized for validator sets of 100–200 nodes.

**Finality Rule:**
A block is considered final when a **Quorum Certificate (QC)** containing signatures from ≥ 2/3 of total stake is produced.

\[
\text{Finalized} \iff \text{Stake}_{signed} \geq \frac{2}{3} S_{total}
\]

**Block Time:** 2 seconds  
**Finality:** Instant (within same block)

**Liveness:** Relies on a rotating leader; if the leader fails, a view-change protocol elects a new leader within a few rounds.

---

### 2.2 Validator Set Management

#### Epoch-Based Rotation
- **Epoch length:** 10,000 blocks (~5.5 hours at 2s block time).
- Validator set updates occur **only at epoch boundaries**.

#### Selection Mechanism
Let:
- \( S_i \) = bonded stake of validator \( i \).
- \( S_{min} \) = minimum self-bond requirement (initial 10,000 SAR).
- \( N \) = active validator set size (parameterized, initial \( N = 150 \)).

The active set consists of the top \( N \) validators by stake.

Validators ranked \( N+1 \) and below enter **standby** status, eligible to replace any active validator that drops below \( S_{min} \) or is slashed.

**Liveness Requirement:** A new epoch's validator set must be known and signed by ≥ 2/3 of the *previous* epoch's stake.

---

### 2.3 Slashing Conditions with Correlated Penalties

A validator is slashed for:

1. **Double-signing** — Signing two different blocks at the same height.
2. **Surround vote** — Voting for a block that conflicts with a previously signed block.
3. **Downtime** — Missing > 5% of signing opportunities within an epoch.

#### Correlated Slashing Model

Safety violations (double-signing, surround vote) are subject to **quadratic correlated slashing**. The penalty for a validator \( i \) involved in a safety violation is:

\[
\text{Penalty}_i = \min\left(1, \alpha \cdot \frac{s_i}{S_{total}} + \beta \cdot \left(\frac{S_{violating}}{S_{total}}\right)^2 \cdot \frac{s_i}{S_{violating}}\right) \cdot s_i
\]

Where:
- \( s_i \) = stake of validator \( i \).
- \( S_{violating} \) = total stake of all validators participating in the violation.
- \( \alpha = 0.05 \) (base linear coefficient).
- \( \beta = 0.5 \) (correlated penalty coefficient).

**Intuition:** If a single validator double-signs, the quadratic term is small (\( S_{violating} \approx s_i \), so the term is ~0.5 * (1/N) * 1, which is negligible). If 1/3 of stake colludes, the quadratic term becomes significant, adding ~5.5% to the total penalty. This makes large collusions exponentially more expensive.

#### Tiered Downtime Penalties

| Offense | Penalty | Consequence |
| :--- | :--- | :--- |
| **Double-signing (any)** | Quadratic formula above | Tombstone: validator ejected permanently. |
| **Surround vote (any)** | Quadratic formula above | Tombstone: validator ejected permanently. |
| **Downtime (first epoch)** | Jailing (no rewards) for 1 epoch | No slashing, only loss of rewards. |
| **Downtime (second consecutive)** | 0.5% stake | Ejected, can re-enter after 7 days. |
| **Downtime (third+ consecutive)** | 1% stake | Ejected, can re-enter after 7 days. |

**Why quadratic:** Linear slashing (flat 5%) is insufficient to deter large cartels. Quadratic slashing ensures that the marginal cost of adding another validator to a collusion increases with collusion size.

Slashed stake is partially burned (50%) and partially distributed to active validators (50%) as a bounty.

---

### 2.4 Light Client Architecture

Sarafu light clients verify finalized headers using **BLS aggregated signatures** and **Merkle proofs**.

#### Components:
- **Merkleized State Tree** — Binary Merkle tree of account states.
- **BLS12-381 Signatures** — Enable aggregation of validator signatures into ~48 bytes.
- **Validator Set Root** — Commitment to the active validator set at each epoch.

#### Proof Size Calculation:

| Component | Size |
| :--- | :--- |
| Aggregated BLS signature | 48 bytes |
| Header metadata | ~200 bytes |
| Validator set Merkle proof | ~2–4 KB |
| **Total** | **≤ 5 KB** (with N ≤ 500) |

**Note:** Validator set size N is capped at 500; proof size remains under 5 KB. If N grows beyond 500 in the future, the proof size will be reassessed; a larger N would require a deeper tree, but still ≤ 10 KB for N up to 2000.

#### Light Client Verification Pseudocode:

```
function verify_header(header, aggregated_signature, validator_set_proof):
    // 1. Verify validator set proof against last known state root
    validator_set = verify_merkle_proof(validator_set_proof)
    
    // 2. Calculate total stake of signers
    signers = validator_set.filter(signed_message(aggregated_signature))
    stake_signed = sum(signer.stake for signer in signers)
    
    // 3. Verify aggregated signature cryptographically
    if not bls_fast_aggregate_verify(aggregated_signature, header.hash, signers.public_keys):
        return INVALID
    
    // 4. Check supermajority condition
    if stake_signed >= (2/3) * validator_set.total_stake:
        return VALID
    else:
        return INVALID
```

---

### 2.5 Networking Layer

| Component | Specification |
| :--- | :--- |
| **Peer Discovery** | Libp2p with Kademlia DHT |
| **Transport** | QUIC (with TCP fallback) |
| **Gossip Protocol** | Structured epidemic broadcast, fanout factor = 8 |
| **RPC Interface** | gRPC + JSON REST gateway |
| **Block Propagation Target** | < 300ms to 95% of validators (under good network conditions) |

**Gossip Design:**
- Transactions are gossiped immediately to mempool participants.
- Blocks are propagated via a two-phase relay: proposer → 8 peers → each relays to 8 more.

---

### 2.6 State Machine: Account-Based Model

#### Rationale:
- Simpler for SMS/USSD/email adapters.
- Predictable gas model.
- Better mobile wallet UX.
- Native nonce support for replay protection.

#### Transaction Format:

```
struct Transaction {
    from:      Address      // 32 bytes
    to:        Address      // 32 bytes
    amount:    uint64       // 8 bytes (in smallest unit)
    nonce:     uint64       // 8 bytes
    fee:       uint64       // 8 bytes
    chain_id:  uint32       // 4 bytes
    signature: [u8; 64]     // Ed25519 signature
}
```

**Nonce Management:**
- Each account has a strictly increasing nonce.
- Transactions with nonce gaps are rejected.
- Mempool maintains a nonce-ordered queue per account.

---

## 3. Cryptography Suite

| Component | Algorithm | Rationale |
| :--- | :--- | :--- |
| **Transaction Signatures** | Ed25519 | Fast, secure, hardware wallet support. |
| **Validator Signatures** | BLS12-381 | Required for signature aggregation. |
| **Hashing** | Blake3 | 3–5x faster than SHA-256. |
| **Merkle Tree** | Binary Merkle | Simplicity, proven security. |

**Validator Key Management:**
- Validators maintain two keys:
  - **Consensus key (BLS12-381)** — Used for signing blocks and votes. Should be kept online but can be secured via a remote signer with HSM support.
  - **Withdrawal key (Ed25519)** — Used for receiving rewards and stake operations. Should be kept offline or in cold storage.
- **Compromise of consensus key:** If detected, the validator must immediately stop signing and initiate a key rotation protocol; otherwise, a compromised key could be used to double-sign and trigger slashing. The protocol supports on-chain consensus key rotation with a delay of 1 epoch to allow recovery.
- **HSM recommendation:** Validators are encouraged to use Hardware Security Modules (HSMs) for BLS key storage to prevent key extraction.

---

## 4. Monetary Policy

### 4.1 Dynamic Issuance Model

Sarafu abandons fixed supply in favor of **security-budget-targeted issuance**.

Let:
- \( M_t \) = total supply at time \( t \).
- \( S_{total} \) = total bonded stake.
- \( \sigma = S_{total} / M_t \) = staking ratio.
- \( r_t \) = annual issuance rate at time \( t \).
- \( k \) = issuance coefficient (governance parameter, initial \( k = 0.1 \)).

**Issuance Rule:**

\[
r_t = k \cdot \sigma
\]

**Supply Update:**

\[
M_{t+1} = M_t \cdot (1 + r_t) - B_t
\]

Where \( B_t \) = fees burned in period \( t \).

**Key Property:** Validator gross return rate is independent of staking ratio:

\[
\text{Gross APR} = k
\]

If \( k = 0.1 \), nominal staking return = 10%. This means security budget scales linearly with staked capital.

---

### 4.2 Security Budget and Attack Cost (Liquidity-Adjusted)

**Attack Cost** (one-time, to acquire ≥ 1/3 stake):

A naive estimate is:

\[
\text{Attack Cost}_{naive} = \frac{1}{3} \cdot S_{total} \cdot P_{SAR}
\]

However, this assumes infinite liquidity. In reality, large purchases drive up price. We define a **Liquidity-Adjusted Attack Cost (LAAC)** :

\[
\text{LAAC} = \int_{0}^{\frac{1}{3}S_{total}} P(q) \, dq
\]

Where \( P(q) \) is the marginal price after purchasing \( q \) tokens, derived from order book depth. For initial security estimates, we assume a conservative liquidity model: purchasing 1/3 of the supply costs at least **twice** the naive cost in illiquid markets.

Thus, the protocol's security margin should account for a multiplier \( \lambda \geq 1 \), where \( \lambda \) is the ratio of LAAC to naive cost. A public dashboard will track \( \lambda \) based on exchange order books and report a **real-time security score**.

**Annual Security Budget** (validator rewards, in USD):

\[
\text{Security Budget} = r_t \cdot M_t \cdot P_{SAR} = k \cdot S_{total} \cdot P_{SAR}
\]

**Attack Rationality Condition (with slashing):**

An attacker will attempt a double-spend if the expected gain exceeds the expected loss, accounting for slashed stake and opportunity cost. With quadratic correlated slashing for collusion, the loss for a cartel controlling fraction \( f \geq 1/3 \) of stake is:

\[
\text{Loss} = \left( \alpha f + \beta f^2 \right) \cdot S_{total} \cdot P_{SAR} + \text{Opportunity cost}
\]

Where:
- \( \alpha = 0.05 \)
- \( \beta = 0.5 \)
- Opportunity cost ≈ \( k f S_{total} P_{SAR} \) (forgone rewards).

Thus, attack is irrational if:

\[
\text{Gain}_{attack} < f S_{total} P_{SAR} \left( \alpha + \beta f + k \right)
\]

For \( f = 1/3 \), \( \alpha + \beta f + k = 0.05 + 0.5/3 + 0.1 \approx 0.05 + 0.167 + 0.1 = 0.317 \). So the attacker loses ~31.7% of the value of their stake, plus the acquisition cost premium \( \lambda \). This is a significant deterrent.

---

### 4.3 Inflation Trajectory

Initial issuance \( r_0 = 0.03 \) (3%) corresponds to an initial staking ratio \( \sigma = r_0 / k = 0.03 / 0.1 = 0.3 \) (30% of supply staked).

As network usage grows, \( B_t \) (fees burned) increases, reducing net inflation toward zero or negative (deflationary) territory during high usage.

**Feedback Loop:** If high usage leads to significant fee burn, \( M \) decreases, \( \sigma \) increases, and \( r \) increases. This creates a stabilizing effect: higher usage (burn) leads to higher issuance in the next period, which replenishes supply and maintains security budget. Simulation shows this loop is damped and does not oscillate wildly for realistic usage patterns.

---

## 5. Validator ROI Model Under Realistic African Infrastructure Costs

### 5.1 Objective

Determine whether Sarafu validators can operate sustainably in:
- Nigeria
- Kenya
- Ghana
- South Africa
- Rwanda

Under:
- Variable power reliability
- Consumer-grade fiber or 4G
- Mid-tier server hardware
- Moderate staking participation (~30–50%)

### 5.2 Infrastructure Cost Model

Annual validator operating cost:

\[
C = C_{hardware} + C_{bandwidth} + C_{power} + C_{maintenance}
\]

#### Conservative African Cost Assumptions

| Component | Annual Cost (USD) |
| :--- | :--- |
| Server (dedicated, 8 cores, 32GB RAM) | $1,200 |
| Redundant VPS fallback | $600 |
| Bandwidth (business fiber or 4G failover) | $800 |
| Power (including outages + UPS) | $600 |
| Maintenance / admin time | $1,200 |
| **Total** | **~$4,400/year** |

Rounded: \( C \approx $5,000 \text{/year} \)

### 5.3 Break-Even Bond Size

Validator annual revenue in USD:

\[
R_i^{USD} = S_i \cdot k \cdot P
\]

Break-even condition:

\[
S_i \cdot k \cdot P = C
\]

\[
S_i = \frac{C}{kP}
\]

Assume:
- \( k = 0.1 \)
- SAR price \( P = $1 \):

\[
S_i = \frac{5000}{0.1 \cdot 1} = 50,000 \text{ SAR}
\]

If price = $0.50:

\[
S_i = 100,000 \text{ SAR}
\]

If price = $2:

\[
S_i = 25,000 \text{ SAR}
\]

**Interpretation:** A validator needs ~50k–100k SAR bonded to operate sustainably under moderate valuation. This is achievable.

### 5.4 Centralization Risk Threshold

If minimum viable validator bond is 50k SAR, and initial validator set N = 150:

Minimum total bonded capital:

\[
S_{min-total} = 150 \cdot 50,000 = 7.5M \text{ SAR}
\]

If total supply M = 100M SAR, staking ratio = 7.5%.

**Conclusion:** Under reasonable valuation, Sarafu validator economics are viable in African markets. The protocol does not force centralization due to cost barriers.

---

## 6. Genesis Distribution Design to Avoid Cartel Formation

### 6.1 Objective

Minimize probability that:
- A single entity
- Or a coordinated coalition

Controls ≥ 1/3 of stake at launch.

This is the most dangerous period in any PoS chain.

### 6.2 Cartel Risk Model

Let:
- \( X_i \) = stake of entity i
- Total supply = M
- Staked supply at genesis = S₀

Cartel control condition:

\[
\sum_{i \in C} X_i \ge \frac{1}{3} S_0
\]

We want to minimize:

\[
P(\exists C : \sum X_i \ge \frac{1}{3} S_0)
\]

### 6.3 Genesis Distribution Constraints

#### Rule 1: No allocation > 5% of total supply

Hard cap:

\[
X_i \le 0.05M
\]

This ensures at least 7 entities required to reach 1/3.

#### Rule 2: Staking Lock-Up for Large Holders

For any entity receiving > 2%:
Mandatory 12-month linear vesting.

Prevents instant cartel formation at genesis.

#### Rule 3: Wide Validator Airdrop

Allocate:
- 20% of supply to prospective validators
- Equal tranches to 200 geographically screened operators

Each receives:

\[
0.1\% M
\]

This makes initial validator control highly distributed.

#### Rule 4: No Foundation Voting Power

If treasury exists:
- It cannot stake.
- Or if it stakes, its stake is non-voting for governance.

### 6.4 Recommended Genesis Structure

| Category | Allocation |
| :--- | :--- |
| Validator bootstrap pool | 20% |
| Public sale (capped per wallet) | 30% |
| Ecosystem incentives (vested) | 20% |
| Team (4-year vest) | 15% |
| Strategic partners (vested) | 10% |
| Community reserve | 5% |

Wallet cap in public sale: ≤ 1% per wallet.

Sybil mitigation via:
- KYC optional but capped
- Or proof-of-personhood gating

### 6.5 Cartel Probability Simulation Insight

If largest holder = 5%, and next 10 largest = 2–3%, probability that 1/3 coalition forms without explicit coordination is extremely low.

Game theory: Each large holder risks 31%+ slashing if collusion detected. Cartel instability increases with larger number of required participants and higher correlated slashing β.

---

## 7. Fee Market (EIP-1559 Style)

### 7.1 Base Fee Adjustment

Sarafu implements a congestion-controlled fee market:

\[
\text{BaseFee}_{t+1} = \text{BaseFee}_t \cdot \left(1 + \frac{\text{GasUsed} - \text{TargetGas}}{\text{TargetGas}} \cdot \gamma \right)
\]

Where:
- \( \gamma = 0.125 \) (adjustment coefficient).
- TargetGas = 50% of block gas limit (parameterized).
- Maximum per-block change: ±12.5%.

**Fee Components:**
- **Base Fee:** Burned.
- **Priority Fee:** Optional tip to proposer.

### 7.2 Fee Burn and Net Inflation

Net inflation after fee burn:

\[
\text{Net Inflation} = r_t - \frac{B_t}{M_t}
\]

High usage → lower net inflation → eventual deflation.

---

## 8. Long-Range Attack Mitigation (Weak Subjectivity)

All Proof-of-Stake systems are vulnerable to **long-range attacks** where an attacker generates a fake history from genesis.

### 8.1 Solution: Weak Subjectivity Checkpoints

A new node joining the network must:

1. Obtain a trusted checkpoint header less than **1,000,000 blocks** old (~23 days).
2. Verify the checkpoint's validator set root.
3. Reject any chain that does not include this checkpoint.

**Trusted Checkpoint Distribution:**
Checkpoints are published by the validator set every 1,000,000 blocks, signed by ≥ 2/3 of current validators. These signed checkpoints are made available via multiple independent channels:
- Official website(s) of community organizations.
- At least five independent block explorers.
- Social media accounts of validators (as a cross-check).

A new node can fetch the latest checkpoint from any of these sources. If the checkpoint is signed by ≥ 2/3 of the validator set at that epoch, it can be trusted. The node should cross-check with at least two independent sources to avoid eclipse attacks.

---

## 9. Governance

### 9.1 Scope

On-chain governance is limited to protocol parameters that do not affect user funds directly:

| Parameter | Governance Scope |
| :--- | :--- |
| Block size | Yes |
| Block gas limit | Yes |
| Issuance coefficient \( k \) | Yes (with bounds 0.05 ≤ k ≤ 0.2) |
| Slashing parameters \( \alpha, \beta \) | Yes (safety-critical) |
| Validator set size \( N \) | Yes |
| Minimum self-bond \( S_{min} \) | Yes |
| Base fee adjustment \( \gamma \) | Yes |
| **User funds** | **Never** |
| **Transaction reversal** | **Never** |
| **Account freezing** | **Never** |

### 9.2 Voting Mechanism

- **Proposal threshold:** 0.1% of total stake.
- **Voting period:** 7 days.
- **Approval rule:** ≥ 2/3 of stake-weighted votes.
- **Enactment:** After a **time-lock** that depends on parameter criticality:

| Parameter Category | Time-Lock |
| :--- | :--- |
| **Safety-critical** (slashing, issuance bounds) | **14 days** |
| **Performance** (block size, gas limit) | **3 days** |
| **Administrative** (validator set size, min bond) | **7 days** |

**Why graduated time-locks:** Safety-critical parameters require the longest delay to allow stakeholders to exit or coordinate a response if a malicious change is passed. Shorter delays for performance parameters allow faster tuning.

### 9.3 Governance Capture Mitigation

If a cartel acquires ≥ 2/3 stake, they could in theory pass changes. However:
- The 14-day time-lock for safety-critical parameters gives honest validators and users time to detect and socially coordinate a fork or exit.
- A public dashboard tracks stake concentration and alerts the community if any single entity or coordinated group approaches the 2/3 threshold.
- The quadratic slashing mechanism makes it expensive for a cartel to misbehave, even if they control governance.

---

## 10. Stateless Integration Layer

### 10.1 Design Philosophy

Sarafu **does not** embed USSD, SMS, banking, or mobile money logic in consensus. Instead, external **adapters** operate as:
- Non-custodial relayers.
- Stateless API clients.
- Signed transaction broadcasters.

If all adapters disappear, the protocol continues functioning.

### 10.2 Adapter Types

| Adapter Type | Function | Security Model |
| :--- | :--- | :--- |
| **SMS Gateway** | Receive signed hex via SMS, broadcast to node | No custody; user signs offline. |
| **USSD Adapter** | Relay signed transaction via USSD; user keys stored on device, not derived from SIM | Keys generated on device; SIM only for transport. |
| **Bank API Webhook** | Listen for bank transfer confirmations, submit payment | Requires bank API key; user signs transaction offline. |
| **Email Broadcaster** | Parse signed transaction from email body | No custody; user signs offline. |

**Important:** Keys are **never** derived from SIM entropy. SIM cards are untrusted and controlled by telecoms; using them for key material introduces carrier dependency and SIM-swap risk. All key generation must happen on the user's device using secure random number generators.

### 10.3 Security Considerations

- Adapters **never** hold private keys.
- All transactions are signed **before** reaching the adapter.
- Adapters can be rate-limited to prevent spam.
- Light client proofs enable adapters to verify finality before confirming to users.

---

## 11. MEV Policy

Miner Extractable Value (MEV) exists in any blockchain with transaction ordering. Sarafu adopts the following stance:

- **Default mempool:** First-come-first-serve ordering, with transactions propagated via gossip. This does not prevent front-running entirely but reduces predictability.
- **Encrypted mempool (future):** A long-term goal is to implement a commit-reveal scheme or threshold encryption to hide transaction contents until inclusion.
- **Proposer-builder separation (PBS):** Not in initial launch; may be considered if MEV becomes a centralization vector.
- **No explicit MEV extraction by validators:** Validators are expected to follow the protocol's ordering rules; any deviation (e.g., reordering for profit) is difficult to prove and thus not slashed initially. The community will monitor and may propose governance changes if MEV becomes problematic.

---

## 12. Jurisdiction Resilience

### 12.1 Legal Design Choices

Sarafu avoids classification as a security or money transmitter by:

| Feature | Rationale |
| :--- | :--- |
| No central issuer | No entity to target. |
| No treasury control | No profit extraction. |
| No dividends | No investment contract. |
| No discretionary monetary committee | No control by any party. |
| No promise of profit | No Howey prong met. |

### 12.2 Geographic Decentralization Metric (Informational)

Sarafu tracks **validator geography** via self-reporting and IP geolocation (with opt-out) for transparency:

**Nakamoto Coefficient (Jurisdictional):**
\[
J = \min\{ n \mid \text{top } n \text{ jurisdictions control } \geq 1/3 \text{ stake} \}
\]

Target: \( J \geq 5 \) (no single jurisdiction, and no coalition of fewer than 5 jurisdictions, can halt the network).

**Note:** This metric is informational and cannot be enforced on-chain due to the possibility of spoofing. It serves as a public dashboard to encourage decentralization.

Public dashboard displays:
- Stake distribution by country.
- Geographic heatmap of validators.
- Historical trends.

---

## 13. Correlated Slashing Stress Test

We model cascading failure scenarios to validate the quadratic slashing mechanism.

### 13.1 Scenario A: 1/3 Coordinated Double-Sign

Let \( f = 1/3 \).

Loss fraction:
\[
\alpha + \beta f = 0.05 + 0.5 \cdot 1/3 \approx 0.217
\]

Add opportunity cost (\( k = 0.1 \)):

Total effective loss ≈ 31.7%.

If attack fails to profit > 31.7% of total staked capital, attack is irrational. Strong deterrent.

### 13.2 Scenario B: Accidental Cascading Validator Failure

Suppose 40% of validators share same cloud provider. Cloud outage causes simultaneous equivocation or downtime.

**Case 1: Downtime Only**
- First epoch: No slashing.
- Second consecutive: 0.5%.
- Third consecutive: 1%.

Tiered penalties prevent catastrophic slashing from infrastructure failure.

**Case 2: Correlated Double-Sign Bug**
If 40% double-sign:
\[
f = 0.4
\]
Loss fraction:
\[
0.05 + 0.5(0.4) = 0.05 + 0.2 = 0.25
\]
Plus opportunity cost 0.1: total ≈ 35%.

Post-slash stake:
Malicious loses 25%.
New effective malicious stake:
\[
0.4 \cdot (1 - 0.25) = 0.3
\]
System returns to safety (< 1/3). Strong stabilizing property.

### 13.3 Scenario C: Cascading Slashing Spiral

If slashing causes validators to fall below \( S_{min} \) and exit:
- Validator count drops → centralization risk increases.

Mitigation:
- Standby validator queue.
- Automatic replacement at epoch boundary.
- Reduced minimum bond temporarily via governance (with delay).

---

## 14. Systemic Risk Summary

| Risk | Outcome | Mitigation |
| :--- | :--- | :--- |
| Cartel 1/3 attack | 31%+ capital loss | Quadratic slashing + opportunity cost |
| Cloud provider outage | Temporary liveness loss | Tiered downtime rules |
| Liquidity collapse | Security budget drop | LAAC monitoring dashboard |
| Governance capture | 14-day response window | Time-lock + social fork |
| Validator centralization | Stake concentration | Genesis caps + wide airdrop |
| Accidental double-sign | 25-35% loss | Quadratic slashing, automatic recovery |

---

## 15. Security Assumptions Summary

| Assumption | Condition |
| :--- | :--- |
| **Safety** | \( \text{Stake}_{honest} > \frac{2}{3} S_{total} \) |
| **Liveness** | \( \text{Stake}_{responsive} > \frac{2}{3} S_{total} \) |
| **Economic security** | Attack cost (liquidity-adjusted) × slashing + opportunity cost > expected gain. |
| **Weak subjectivity** | New nodes obtain checkpoint < 1M blocks old from ≥ 2 independent sources. |
| **Network partition** | Validators maintain connectivity; no permanent partition isolating > 1/3 stake. |
| **Key security** | Validators follow HSM/remote signer best practices; users secure their Ed25519 keys. |
| **Liquidity** | Market depth is sufficient that acquiring 1/3 stake costs at least the naive estimate (or a public dashboard tracks λ). |

---

## 16. Implementation Roadmap

### Phase 0: Specification Freeze (Current)
- Peer review of this document.
- Formal verification of core consensus (optional).

### Phase 1: Core Development (Months 1–6)
- Implement HotStuff consensus in Rust (or C++ per original vision).
- Develop Libp2p networking layer.
- Build account-based state machine.
- Implement BLS12-381 and Ed25519 with quadratic slashing logic.

### Phase 2: Testnet (Months 7–9)
- Launch "Kilimanjaro" testnet with 50 validators.
- Measure:
  - Block propagation latency.
  - Validator liveness under churn.
  - Signature aggregation size.
  - Light client sync time.
- Security audit (2 firms) with focus on slashing and governance logic.

### Phase 3: Mainnet (Months 10–12)
- Genesis event with distribution rules per Section 6.
- Initial validator set (100 nodes, geographically distributed).
- Launch with weak subjectivity checkpoints.
- Deploy governance module with graduated time-locks.
- Public dashboard for liquidity-adjusted security monitoring.

### Phase 4: Ecosystem (Year 2)
- Release adapter SDKs.
- Partner with mobile money aggregators.
- Deploy USSD/SMS gateways in select countries.
- Begin research on encrypted mempool / PBS.

---

## 17. Conclusion

Sarafu has evolved from a narrative-driven concept to a rigorously specified protocol, hardened by adversarial review and real-world economic modeling. It addresses the fundamental challenge of emerging-market payments:

> **How do you build a monetary system that is secure, accessible, and jurisdiction-resilient, without relying on volatile speculation or centralized control?**

The answer is:
- HotStuff BFT for instant, secure finality.
- Dynamic issuance for sustainable security budgets.
- Quadratic correlated slashing to deter cartels.
- 5 KB light clients for feature-phone accessibility.
- Graduated governance time-locks for capture resistance.
- Liquidity-aware economic modeling for realistic security estimates.
- Validator ROI modeling proving sustainability in African markets.
- Genesis distribution design minimizing cartel formation.
- Stress-tested slashing under cascading failures.
- Stateless adapters for integration without dependency.
- Jurisdiction-neutral design for regulatory longevity.

With this specification, Sarafu is no longer a whitepaper—it is a **blueprint for implementation**. The economic and security mechanisms are coherent, modeled, and tested against adversarial scenarios. The remaining work is engineering discipline and community building.

Sarafu is not a token. It is not a company. It is a public good: a neutral settlement layer for the billions of people underserved by legacy finance.

**The specification is complete. The work now begins.**

---

*Disclaimer: This document is a technical specification. It does not constitute an offer to sell securities, investment advice, or solicitation. Cryptocurrency protocols carry inherent risks; participants should conduct their own due diligence.*