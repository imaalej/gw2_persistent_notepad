#ifndef NOTEPAD_H
#define NOTEPAD_H

#include <vector>
#include <string>

class Notepad {
public:
    struct Note {
        int mId;
        bool isDirty;
        bool charSpecific;
        std::string noteText;
    };

    Notepad();
    void addNote(bool isToonSpecific);
    void removeNote(int aid);
    int getLength();
    
private:
    std::vector<Note> notes;
    int mIdCounter;
};

#endif
