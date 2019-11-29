#ifndef AUTOMATON_H
#define AUTOMATON_H

#include <map>
#include <vector>
#include <QChar>
#include <QString>

class Automaton
{
private:
    struct State
    {
        bool terminal;
        std::map<QChar, int> transitions;
    };

private:
    explicit Automaton(size_t size);

public:
    static Automaton fromString(const QString &string);
    bool step(const QChar &current, bool autoReset = false);
    void reset();
    bool isTerminal() const;

private:
    int activeStateIdx;
    std::vector<State> states;
};

#endif // AUTOMATON_H
