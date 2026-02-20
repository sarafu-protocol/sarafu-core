#include "sarafu/state/account_manager.h"

namespace sarafu {
namespace state {

AccountManager::AccountManager() = default;

AccountManager::~AccountManager() = default;

Account AccountManager::get_account(const Address& address) const {
    auto it = accounts_.find(address);
    if (it != accounts_.end()) {
        return it->second;
    }
    
    // Return a new account with zero balance and nonce 0
    return Account(address, 0, 0);
}

Account AccountManager::create_account(const Address& address, uint64_t initial_balance) {
    Account account(address, initial_balance, 0);
    accounts_[address] = account;
    return account;
}

void AccountManager::update_account(const Account& account) {
    accounts_[account.address] = account;
}

bool AccountManager::validate_nonce(const Address& address, uint64_t transaction_nonce) const {
    auto account = get_account(address);
    return account.nonce == transaction_nonce;
}

void AccountManager::increment_nonce(const Address& address) {
    auto account = get_account(address);
    account.nonce++;
    update_account(account);
}

bool AccountManager::has_sufficient_balance(const Address& address, uint64_t required_amount) const {
    auto account = get_account(address);
    return account.balance >= required_amount;
}

void AccountManager::deduct_balance(const Address& address, uint64_t amount) {
    auto account = get_account(address);
    account.balance -= amount;
    update_account(account);
}

void AccountManager::add_balance(const Address& address, uint64_t amount) {
    auto account = get_account(address);
    account.balance += amount;
    update_account(account);
}

uint64_t AccountManager::get_nonce(const Address& address) const {
    auto account = get_account(address);
    return account.nonce;
}

uint64_t AccountManager::get_balance(const Address& address) const {
    auto account = get_account(address);
    return account.balance;
}

bool AccountManager::account_exists(const Address& address) const {
    return accounts_.find(address) != accounts_.end();
}

const std::map<Address, Account>& AccountManager::get_all_accounts() const {
    return accounts_;
}

void AccountManager::clear() {
    accounts_.clear();
}

} // namespace state
} // namespace sarafu
