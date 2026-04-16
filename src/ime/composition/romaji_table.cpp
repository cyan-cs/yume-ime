// Copyright (c) 2026-present cyan-cs
//
// Licensed under the MIT License.
// https://opensource.org/license/mit
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#include "ime/composition/romaji_table.hpp"

#include <array>
#include <utility>

namespace yume::ime::composition {

namespace {

using Entry = std::pair<std::u16string_view, std::u16string_view>;

constexpr Entry kEntries[] = {
    {u"a", u"\u3042"}, {u"i", u"\u3044"}, {u"u", u"\u3046"}, {u"e", u"\u3048"}, {u"o", u"\u304A"},
    {u"ka", u"\u304B"}, {u"ki", u"\u304D"}, {u"ku", u"\u304F"}, {u"ke", u"\u3051"}, {u"ko", u"\u3053"},
    {u"ga", u"\u304C"}, {u"gi", u"\u304E"}, {u"gu", u"\u3050"}, {u"ge", u"\u3052"}, {u"go", u"\u3054"},
    {u"sa", u"\u3055"}, {u"shi", u"\u3057"}, {u"si", u"\u3057"}, {u"su", u"\u3059"}, {u"se", u"\u305B"}, {u"so", u"\u305D"},
    {u"za", u"\u3056"}, {u"ji", u"\u3058"}, {u"zi", u"\u3058"}, {u"zu", u"\u305A"}, {u"ze", u"\u305C"}, {u"zo", u"\u305E"},
    {u"ta", u"\u305F"}, {u"chi", u"\u3061"}, {u"ti", u"\u3061"}, {u"tsu", u"\u3064"}, {u"tu", u"\u3064"}, {u"te", u"\u3066"}, {u"to", u"\u3068"},
    {u"da", u"\u3060"}, {u"di", u"\u3062"}, {u"du", u"\u3065"}, {u"de", u"\u3067"}, {u"do", u"\u3069"},
    {u"na", u"\u306A"}, {u"ni", u"\u306B"}, {u"nu", u"\u306C"}, {u"ne", u"\u306D"}, {u"no", u"\u306E"},
    {u"ha", u"\u306F"}, {u"hi", u"\u3072"}, {u"fu", u"\u3075"}, {u"hu", u"\u3075"}, {u"he", u"\u3078"}, {u"ho", u"\u307B"},
    {u"ba", u"\u3070"}, {u"bi", u"\u3073"}, {u"bu", u"\u3076"}, {u"be", u"\u3079"}, {u"bo", u"\u307C"},
    {u"pa", u"\u3071"}, {u"pi", u"\u3074"}, {u"pu", u"\u3077"}, {u"pe", u"\u307A"}, {u"po", u"\u307D"},
    {u"ma", u"\u307E"}, {u"mi", u"\u307F"}, {u"mu", u"\u3080"}, {u"me", u"\u3081"}, {u"mo", u"\u3082"},
    {u"ya", u"\u3084"}, {u"yu", u"\u3086"}, {u"yo", u"\u3088"},
    {u"ra", u"\u3089"}, {u"ri", u"\u308A"}, {u"ru", u"\u308B"}, {u"re", u"\u308C"}, {u"ro", u"\u308D"},
    {u"wa", u"\u308F"}, {u"wi", u"\u3046\u3043"}, {u"we", u"\u3046\u3047"}, {u"wo", u"\u3092"}, {u"nn", u"\u3093"}, {u"xn", u"\u3093"},
    {u"n'a", u"\u3093\u3042"}, {u"n'i", u"\u3093\u3044"}, {u"n'u", u"\u3093\u3046"},
    {u"n'e", u"\u3093\u3048"}, {u"n'o", u"\u3093\u304A"},
    {u"n'ya", u"\u3093\u3084"}, {u"n'yu", u"\u3093\u3086"}, {u"n'yo", u"\u3093\u3088"},
    {u"kya", u"\u304D\u3083"}, {u"kyu", u"\u304D\u3085"}, {u"kyo", u"\u304D\u3087"},
    {u"kye", u"\u304D\u3047"}, {u"qwa", u"\u304F\u3041"}, {u"qwi", u"\u304F\u3043"}, {u"qwu", u"\u304F\u3045"}, {u"qwe", u"\u304F\u3047"}, {u"qwo", u"\u304F\u3049"},
    {u"kwa", u"\u304F\u3041"}, {u"kwi", u"\u304F\u3043"}, {u"kwu", u"\u304F\u3045"}, {u"kwe", u"\u304F\u3047"}, {u"kwo", u"\u304F\u3049"},
    {u"gya", u"\u304E\u3083"}, {u"gyu", u"\u304E\u3085"}, {u"gyo", u"\u304E\u3087"},
    {u"gye", u"\u304E\u3047"}, {u"gwa", u"\u3050\u3041"}, {u"gwi", u"\u3050\u3043"}, {u"gwu", u"\u3050\u3045"}, {u"gwe", u"\u3050\u3047"}, {u"gwo", u"\u3050\u3049"},
    {u"sha", u"\u3057\u3083"}, {u"sya", u"\u3057\u3083"}, {u"shu", u"\u3057\u3085"}, {u"syu", u"\u3057\u3085"}, {u"sho", u"\u3057\u3087"}, {u"syo", u"\u3057\u3087"}, {u"sye", u"\u3057\u3047"},
    {u"ja", u"\u3058\u3083"}, {u"jya", u"\u3058\u3083"}, {u"ju", u"\u3058\u3085"}, {u"jyu", u"\u3058\u3085"}, {u"jo", u"\u3058\u3087"}, {u"jyo", u"\u3058\u3087"}, {u"jye", u"\u3058\u3047"},
    {u"zya", u"\u3058\u3083"}, {u"zyu", u"\u3058\u3085"}, {u"zyo", u"\u3058\u3087"}, {u"zye", u"\u3058\u3047"},
    {u"cha", u"\u3061\u3083"}, {u"cya", u"\u3061\u3083"}, {u"tya", u"\u3061\u3083"}, {u"chu", u"\u3061\u3085"}, {u"cyu", u"\u3061\u3085"}, {u"tyu", u"\u3061\u3085"}, {u"cho", u"\u3061\u3087"}, {u"cyo", u"\u3061\u3087"}, {u"tyo", u"\u3061\u3087"}, {u"cye", u"\u3061\u3047"}, {u"tye", u"\u3061\u3047"},
    {u"nya", u"\u306B\u3083"}, {u"nyu", u"\u306B\u3085"}, {u"nyo", u"\u306B\u3087"}, {u"nye", u"\u306B\u3047"},
    {u"hya", u"\u3072\u3083"}, {u"hyu", u"\u3072\u3085"}, {u"hyo", u"\u3072\u3087"}, {u"hye", u"\u3072\u3047"},
    {u"bya", u"\u3073\u3083"}, {u"byu", u"\u3073\u3085"}, {u"byo", u"\u3073\u3087"}, {u"bye", u"\u3073\u3047"},
    {u"pya", u"\u3074\u3083"}, {u"pyu", u"\u3074\u3085"}, {u"pyo", u"\u3074\u3087"}, {u"pye", u"\u3074\u3047"},
    {u"mya", u"\u307F\u3083"}, {u"myu", u"\u307F\u3085"}, {u"myo", u"\u307F\u3087"}, {u"mye", u"\u307F\u3047"},
    {u"rya", u"\u308A\u3083"}, {u"ryu", u"\u308A\u3085"}, {u"ryo", u"\u308A\u3087"}, {u"rye", u"\u308A\u3047"},
    {u"xya", u"\u3083"}, {u"xyu", u"\u3085"}, {u"xyo", u"\u3087"}, {u"xwa", u"\u308E"}, {u"lwa", u"\u308E"},
    {u"xa", u"\u3041"}, {u"xi", u"\u3043"}, {u"xu", u"\u3045"}, {u"xe", u"\u3047"}, {u"xo", u"\u3049"},
    {u"la", u"\u3041"}, {u"li", u"\u3043"}, {u"lu", u"\u3045"}, {u"le", u"\u3047"}, {u"lo", u"\u3049"},
    {u"xtsu", u"\u3063"}, {u"ltsu", u"\u3063"}, {u"ltu", u"\u3063"},
    {u"she", u"\u3057\u3047"}, {u"je", u"\u3058\u3047"}, {u"che", u"\u3061\u3047"},
    {u"thi", u"\u3066\u3043"}, {u"the", u"\u3066\u3047"}, {u"tho", u"\u3066\u3087"},
    {u"dhi", u"\u3067\u3043"}, {u"dhe", u"\u3067\u3047"}, {u"dho", u"\u3067\u3087"},
    {u"tsa", u"\u3064\u3041"}, {u"tsi", u"\u3064\u3043"}, {u"tse", u"\u3064\u3047"}, {u"tso", u"\u3064\u3049"},
    {u"fa", u"\u3075\u3041"}, {u"fi", u"\u3075\u3043"}, {u"fe", u"\u3075\u3047"}, {u"fo", u"\u3075\u3049"},
    {u"fya", u"\u3075\u3083"}, {u"fyu", u"\u3075\u3085"}, {u"fyo", u"\u3075\u3087"},
    {u"wha", u"\u3046\u3041"}, {u"whi", u"\u3046\u3043"}, {u"whe", u"\u3046\u3047"}, {u"who", u"\u3046\u3049"},
    {u"va", u"\u3094\u3041"}, {u"vi", u"\u3094\u3043"}, {u"vu", u"\u3094"}, {u"ve", u"\u3094\u3047"}, {u"vo", u"\u3094\u3049"}
};

std::vector<TrieNode> buildTrie() {
    std::vector<TrieNode> nodes;
    nodes.reserve(256);
    nodes.emplace_back();

    for (const auto& entry : kEntries) {
        int32_t currentIndex = 0;
        for (char16_t ch : entry.first) {
            const int slot = TrieNode::childIndex(ch);
            if (slot < 0) {
                continue;
            }

            int32_t nextIndex = nodes[static_cast<size_t>(currentIndex)].children[static_cast<size_t>(slot)];
            if (nextIndex == TrieNode::kNoChild) {
                nextIndex = static_cast<int32_t>(nodes.size());
                nodes[static_cast<size_t>(currentIndex)].children[static_cast<size_t>(slot)] = nextIndex;
                nodes.emplace_back();
            }
            currentIndex = nextIndex;
        }
        nodes[static_cast<size_t>(currentIndex)].value = entry.second;
    }

    return nodes;
}

}

int TrieNode::childIndex(char16_t ch) {
    if (ch >= u'a' && ch <= u'z') {
        return static_cast<int>(ch - u'a');
    }
    if (ch == u'\'') {
        return 26;
    }
    return -1;
}

const std::vector<TrieNode>& RomajiTable::getNodes() {
    static const std::vector<TrieNode> nodes = buildTrie();
    return nodes;
}

}
