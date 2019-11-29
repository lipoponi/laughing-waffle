#include "automaton.h"

Automaton::Automaton(size_t size = 1)
    : activeStateIdx(0)
    , states(size)
{}

Automaton Automaton::fromString(const QString &str)
{
    Automaton result;
    std::vector<int> suffix(str.size() + 1, 0);
    suffix[0] = -1;

    for (int i = 0; i < str.size(); i++) {
        QChar ch = str.at(i);

        int p = suffix[i];
        while (p != -1 && str.at(p) != ch) {
            p = suffix[p];
        }
        suffix[i + 1] = p + 1;
    }

    result.states.resize(str.size() + 1);
    for (int i = 0; i < str.size(); i++) {
        result.states[i].transitions[str.at(i)] = i + 1;
    }

    for (size_t i = 1; i < suffix.size(); i++) {
        State &suf = result.states[suffix[i]];
        State &state = result.states[i];
        for (auto &trans : suf.transitions) {
            QChar ch = trans.first;
            if (state.transitions.count(ch) == 0) {
                state.transitions[ch] = trans.second;
            }
        }
    }

    result.states.back().terminal = true;

    return result;
}

bool Automaton::step(const QChar &current, bool autoReset)
{
    State &activeState = states[activeStateIdx];
    if (activeState.transitions.count(current) == 0) {
        if (autoReset) {
            reset();
        }
        return false;
    }

    activeStateIdx = activeState.transitions[current];
    return true;
}

void Automaton::reset()
{
    activeStateIdx = 0;
}

bool Automaton::isTerminal() const
{
    return states[activeStateIdx].terminal;
}
