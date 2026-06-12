#include <iostream>
#include <vector>

class Notepad{
    public:
        // Struct for individual notes
        struct NoteInstance{
            int mId;
            bool isDirty;
            bool charSpecific;
            std::string noteText;
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
    
    private:
        std::vector<NoteInstance> notes;
        int mIdCounter;
};
