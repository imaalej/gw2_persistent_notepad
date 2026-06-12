#include <iostream>
#include <vector>
#include "json.hpp"

class Notepad{
    public:
        // Struct for individual notes
        struct NoteInstance{
            int mId;
            bool isDirty;
            bool charSpecific; // depend on colour coding
            // and if character specific, how do we remember that to store and check?
            std::string noteText;

            //macro to json map individual notes
            NLOHMANN_DEFINE_TYPE_INTRUSIVE(NoteInstance, mId, isDirty, charSpecific, noteText)
        };

        // Constructor
        Notepad(){
            // default note
            mIdCounter = 0;
            notes.push_back({mIdCounter++, false, false, "Type anything here..."});
        }

        void addNoteInstance(bool aCharSpecific){
            notes.push_back({mIdCounter++,false, aCharSpecific, "Type anything here..."});

        }

        void removeNoteInstance(int aId){
            for (auto it = notes.begin(); it != notes.end(); ++it) {
                if (it->mId == aId){
                    notes.erase(it);
                    return;
                }
            }
        }

        int getLength(){
            return notes.size();
        }
    
    private:
        std::vector<NoteInstance> notes;
        int mIdCounter;

        //macro to serialize vector
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Notepad, notes, mIdCounter)
};
