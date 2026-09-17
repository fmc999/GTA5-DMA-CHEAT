#pragma once

// ============================================================================
// VehicleNames.h —— 载具模型哈希 → 型号名 / 中文名 / 分类（自动生成，勿手改）
//
// 生成脚本：_uiwork/gen_vehicle_names_h.py
// 哈希算法：joaat（小写模型名），与 RuntimeTables::Joaat 同算法；
// 已用 tunables.bin 交叉校验：rhino=0x2EA68690、deluxo=0x586765FB、adder=0xB779A091 全对。
//
// 原 VehicleList.cpp 里的手写表有多条哈希/名字对不上（例：把 adder 的 0xB779A091
// 标成了 Zentorno、Deluxo 标成 0xB92E7E0F），本表全部按 joaat 重算并扩表。
// 只读展示用：不写任何内存。
// ============================================================================

#include <cstdint>

struct VehicleNameEntry
{
    uint32_t    hash;
    const char* model;   // 模型名（英文）
    const char* cn;      // 中文名
    const char* kind;    // 分类（空/直升机/装甲/特殊/车/摩托/自行车/服务）
};

// 条目数：122
inline const VehicleNameEntry kVehicleNames[] = {
    { 0xB779A091u, "adder", "蝰蛇", "车" },
    { 0x46699F47u, "akula", "阿库拉", "直升机" },
    { 0x63ABADE7u, "akuma", "恶鬼", "摩托" },
    { 0xEA313705u, "alkonost", "信天翁", "空" },
    { 0x45D56ADAu, "ambulance", "救护车", "服务" },
    { 0x31F0B376u, "annihilator", "歼灭者", "直升机" },
    { 0x11962E49u, "annihilator2", "歼灭者隐形", "直升机" },
    { 0x2189D250u, "apc", "APC 装甲车", "装甲" },
    { 0xED552C74u, "autarch", "独裁者", "车" },
    { 0x81BD2ED0u, "avenger", "复仇者", "空" },
    { 0x2CB1C1F4u, "b11", "B-11 打击者", "空" },
    { 0x25C5AF13u, "banshee2", "女妖 900R", "车" },
    { 0xF34DFB25u, "barrage", "弹幕", "装甲" },
    { 0xF9300CC5u, "bati", "巴提 801", "摩托" },
    { 0xCADD5D2Du, "bati2", "巴提 801RR", "摩托" },
    { 0xA1355F67u, "blazer5", "水陆摩托", "特殊" },
    { 0x43779C54u, "bmx", "BMX", "自行车" },
    { 0x28F07CBAu, "brute", "布鲁特垃圾车", "服务" },
    { 0x9AE6DDA1u, "bullet", "子弹", "车" },
    { 0x2F03547Bu, "buzzard", "兀鹰", "直升机" },
    { 0x2C75F0DDu, "buzzard2", "兀鹰（无武装）", "直升机" },
    { 0x15F27762u, "cargoplane", "货运飞机", "空" },
    { 0xB1D95DA0u, "cheetah", "猎豹", "车" },
    { 0xD6BC7523u, "chernobog", "切尔诺伯格", "装甲" },
    { 0x276D98A3u, "comet5", "彗星 SR", "车" },
    { 0xFE5F0722u, "deathbike", "死亡摩托", "特殊" },
    { 0x586765FBu, "deluxo", "德罗索", "特殊" },
    { 0x5EE005DAu, "deveste", "恶魔十六", "车" },
    { 0xCA495705u, "dodo", "渡渡鸟", "空" },
    { 0x711D4738u, "dune3", "沙丘 FAV", "装甲" },
    { 0xDE3D9D22u, "elegy2", "挽歌改装版", "车" },
    { 0x4EE74355u, "emerus", "埃梅鲁斯", "车" },
    { 0x8198AEDCu, "entity2", "实体 XF", "车" },
    { 0x6838FC1Du, "entity3", "实体 MT", "车" },
    { 0x9229E4EBu, "faggio", "小绵羊", "摩托" },
    { 0x73920F8Eu, "firetruk", "消防车", "服务" },
    { 0x2C634FBDu, "frogger", "青蛙", "直升机" },
    { 0x3944D5A0u, "furia", "狂怒", "车" },
    { 0x4B6C568Au, "hakuchou", "隼", "摩托" },
    { 0xF0C2A91Fu, "hakuchou2", "隼 改装版", "摩托" },
    { 0xFE141DA6u, "halftrack", "半履带车", "装甲" },
    { 0x5A82F9AEu, "hauler", "牵引车", "服务" },
    { 0x89BA59F5u, "havok", "浩劫", "直升机" },
    { 0xFD707EDEu, "hunter", "猎手", "直升机" },
    { 0x39D6E83Fu, "hydra", "九头蛇", "空" },
    { 0x9114EADAu, "insurgent", "叛乱者", "装甲" },
    { 0x8D4B7A8Au, "insurgent3", "叛乱者改装版", "装甲" },
    { 0x85E8E76Bu, "italigtb", "意大利 GTB", "车" },
    { 0xAA6F980Au, "khanjali", "可汗贾利", "装甲" },
    { 0xD86A0247u, "krieger", "克里格", "车" },
    { 0xAE2BFE94u, "kuruma", "骷髅马", "装甲" },
    { 0x187D938Du, "kuruma2", "骷髅马装甲版", "装甲" },
    { 0xB39B0AE6u, "lazer", "天煞", "空" },
    { 0xF92AEC4Du, "limo2", "武装礼车", "装甲" },
    { 0xA5325278u, "manchez", "曼切斯", "摩托" },
    { 0x9D0450CAu, "maverick", "小牛", "直升机" },
    { 0x79DD18AEu, "menacer", "威胁者", "装甲" },
    { 0xB53C6C52u, "minitank", "入侵者", "特殊" },
    { 0x5D56F01Bu, "molotok", "锤头鲨", "空" },
    { 0x35ED670Bu, "mule", "骡子", "服务" },
    { 0x4131F378u, "nero2", "尼禄改装版", "车" },
    { 0x19DD9ED1u, "nightshark", "夜鲨", "装甲" },
    { 0x3DC92356u, "nokota", "野马", "空" },
    { 0x34B82784u, "oppressor", "暴君", "特殊" },
    { 0x7B54A9D3u, "oppressor2", "暴君 Mk2", "特殊" },
    { 0x767164D6u, "osiris", "欧西里斯", "车" },
    { 0x32265F8Du, "p45nokota", "P-45 野马", "空" },
    { 0x9734F3EAu, "penetrator", "穿透者", "车" },
    { 0x809AA4CBu, "phantom", "幻影", "服务" },
    { 0x79FBB0C5u, "police", "警车", "服务" },
    { 0x9F05F101u, "police2", "警车 2", "服务" },
    { 0x7DE35E7Du, "pounder", "重击者", "服务" },
    { 0x7E8F677Fu, "prototipo", "原型 X80", "车" },
    { 0xAD6065C0u, "pyro", "火蜂", "空" },
    { 0xEEF345ECu, "rcbandito", "遥控小坦克", "特殊" },
    { 0x0DF381E5u, "reaper", "死神", "车" },
    { 0x2EA68690u, "rhino", "犀牛坦克", "装甲" },
    { 0xB822A1AAu, "riot", "防暴车", "服务" },
    { 0xC5DD6967u, "rogue", "流氓", "空" },
    { 0x381E10BDu, "ruiner2", "蹂躏者 2000", "特殊" },
    { 0xECA6B6A3u, "s80", "S80", "车" },
    { 0xA960B13Eu, "sanchez2", "桑切斯", "摩托" },
    { 0xFB133A17u, "savage", "野蛮人", "直升机" },
    { 0xD9F0503Du, "scramjet", "火箭狂雷", "特殊" },
    { 0xE8983F9Fu, "seabreeze", "海风", "空" },
    { 0xD4AE63D9u, "seasparrow", "海雀", "直升机" },
    { 0xE7D2A16Eu, "shotaro", "创战纪摩托", "特殊" },
    { 0xA6951D5Cu, "sparrow", "麻雀", "直升机" },
    { 0x9A9EB7DEu, "starling", "八哥", "空" },
    { 0x6827CF72u, "stockade", "装甲车", "服务" },
    { 0x64DE07A1u, "strikeforce", "B-11 打击者", "空" },
    { 0x34DBA661u, "stromberg", "斯特龙伯格", "特殊" },
    { 0x39DA2754u, "sultan", "苏丹", "车" },
    { 0xEE6024BCu, "sultanrs", "苏丹 RS", "车" },
    { 0x2A54C47Du, "supervolito", "超级伏里托", "直升机" },
    { 0xEBC24DF2u, "swift", "雨燕", "直升机" },
    { 0x6322B39Au, "t20", "T20", "车" },
    { 0x744CA80Du, "taco", "快餐车", "服务" },
    { 0xBC5DC07Eu, "taipan", "太攀", "车" },
    { 0x83051506u, "technical", "技术型", "装甲" },
    { 0x4662BCBBu, "technical2", "技术型水陆", "装甲" },
    { 0x50D4D19Fu, "technical3", "技术型改装版", "装甲" },
    { 0x1044926Fu, "tempesta", "风暴", "车" },
    { 0x3E3D1F59u, "thrax", "色雷斯", "车" },
    { 0x58CDAF30u, "thruster", "推进器", "特殊" },
    { 0x4B89C901u, "tm02khanjali", "可汗贾利", "装甲" },
    { 0x56C8A5EFu, "toreador", "图拉尔多", "特殊" },
    { 0x8FD54EBBu, "trailersmall2", "防空拖车", "装甲" },
    { 0x3E2E4F8Au, "tula", "巨嘴鸟", "空" },
    { 0x185484E1u, "turismor", "图里斯莫", "车" },
    { 0x7B406EFBu, "tyrus", "泰勒斯", "车" },
    { 0x7397224Cu, "vagner", "瓦格纳", "车" },
    { 0xA09E15FDu, "valkyrie", "女武神", "直升机" },
    { 0x403820E8u, "velum2", "维鲁姆", "空" },
    { 0xB5EF4C33u, "vigilante", "义警", "特殊" },
    { 0xC4810400u, "visione", "幻视", "车" },
    { 0x920016F1u, "volatus", "鹤", "直升机" },
    { 0x3AF76F4Au, "voltic2", "火箭车", "特殊" },
    { 0x8E08EC82u, "wastelander", "水陆车", "特殊" },
    { 0x36B4A8A9u, "xa21", "XA-21", "车" },
    { 0xAC5DF515u, "zentorno", "精灵", "车" },
    { 0xD757D97Du, "zorrusso", "佐鲁索", "车" },
};

// 按哈希查表；没收录返回 nullptr
inline const VehicleNameEntry* LookupVehicleName(uint32_t hash)
{
    for (const VehicleNameEntry& e : kVehicleNames)
    {
        if (e.hash == hash)
            return &e;
    }
    return nullptr;
}
