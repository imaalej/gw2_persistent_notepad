#include  "notes.h"
#include <iostream>
#include "json.hpp"

// Constructor
Notepad::Notepad(){
    // default note
    mIdCounter = 0;
    notes.push_back({mIdCounter++, false, "Type anything here..."});
}

void Notepad::addNoteInstance(bool aCharSpecific){
    notes.push_back({mIdCounter++, aCharSpecific, "Type anything here..."});

}

void Notepad::removeNoteInstance(int aId){
    for (auto it = notes.begin(); it != notes.end(); ++it) {
        if (it->mId == aId){
            notes.erase(it);
            return;
        }
    }
}

int Notepad::getLength(){
    return notes.size();
}

std::string Notepad::getNoteText(int aId){
    for (const auto& note : notes) {
        if (note.mId == aId) {
            return note.noteText;
        }
    }
    return "";
}

void Notepad::setNoteText(int aId, const std::string& aText){
    for (auto& note : notes) {
        if (note.mId == aId) {
            note.noteText = aText;
            return;
        }
    }
}
