#pragma once


static auto is_act_true = [](auto& i) { return i.is_active; };
static auto is_not_act_true = [](auto& i) { return !i.is_active; };