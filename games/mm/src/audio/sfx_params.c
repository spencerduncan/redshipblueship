#include "global.h"

#define DEFINE_SFX(_0, importance, distParam, randParam, flags2, flags1)           \
    { importance, flags2,                                                          \
      ((((distParam) << SFX_PARAM_DIST_RANGE_SHIFT) & SFX_PARAM_DIST_RANGE_MASK) | \
       (((randParam) << SFX_PARAM_RAND_FREQ_RAISE_SHIFT) & SFX_PARAM_RAND_FREQ_RAISE_MASK) | (flags1)) },

SfxParams MM_sEnemyBankParams[] = {
#include "tables/sfx/enemybank_table.h"
};

SfxParams MM_sPlayerBankParams[] = {
#include "tables/sfx/playerbank_table.h"
};

SfxParams MM_sItemBankParams[] = {
#include "tables/sfx/itembank_table.h"
};

SfxParams MM_sEnvBankParams[] = {
#include "tables/sfx/environmentbank_table.h"
};

SfxParams MM_sSystemBankParams[] = {
#include "tables/sfx/systembank_table.h"
};

SfxParams MM_sOcarinaBankParams[] = {
#include "tables/sfx/ocarinabank_table.h"
};

SfxParams MM_sVoiceBankParams[] = {
#include "tables/sfx/voicebank_table.h"
};

#undef DEFINE_SFX

SfxParams* gSfxParams[7] = {
    MM_sPlayerBankParams, MM_sItemBankParams,    MM_sEnvBankParams,   MM_sEnemyBankParams,
    MM_sSystemBankParams, MM_sOcarinaBankParams, MM_sVoiceBankParams,
};
