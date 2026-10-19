#pragma once
namespace integration {
// Item effects share avatar storage in every mode. Only chair behavior has
// a different owner when MapleRoot supplies its offsets and Discord hooks.
template <class Storage, class Chairs, class Effects>
void InstallAvatarFeatures(bool mapleRootChairs, Storage storage, Chairs chairs, Effects effects) {
    storage();
    if (!mapleRootChairs) chairs();
    effects();
}
}
