
#ifndef VEIL_EC_COMMITMENT_H
#define VEIL_EC_COMMITMENT_H

#include <uint256.h>
#include <secp256k1.h>
#include <secp256k1/include/secp256k1_generator.h>

#define EC_COMMITMENT_SIZE 33

class EcCommitment
{
public:
    std::vector<unsigned char> m_vchCommitment;

    void SetNull()
    {
        m_vchCommitment.clear();
        m_vchCommitment.resize(EC_COMMITMENT_SIZE);
    }

    EcCommitment() { SetNull(); }
    EcCommitment(const secp256k1_context* ctx, const secp256k1_generator* secp256k1_generator_h, const int64_t& nValue, const uint256& blind);

    std::vector<unsigned char> GetCommitmentValue() const { return m_vchCommitment; }
    const unsigned char* begin() const { return m_vchCommitment.data(); }
};

#endif //VEIL_EC_COMMITMENT_H
