#ifndef NOTEPAD_H
#define NOTEPAD_H

#include <vector>
#include <string>
#include "json.hpp"

class Notepad {
public:
    struct NoteInstance {
        int mId;
        bool isDirty;
        // charSpecific based on colour coding?
        // How do we remember that and store and check per character?
        bool charSpecific;
        std::string noteText;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(NoteInstance, mId, isDirty, charSpecific, noteText)
    };

    Notepad();
    void addNoteInstance(bool isToonSpecific);
    void removeNoteInstance(int aId);
    void clearDirtyFlag(int aId);
    std::string getNoteText(int aId);
    void setNoteText(int aId, const std::string& aText);
    int getLength();
    
    std::vector<NoteInstance> notes;
    int mIdCounter;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Notepad, notes, mIdCounter)
};

#endif
