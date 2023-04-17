struct GEdge {
    int orientation;
    int top, bot;
    float m, b; // x = m * y + b

    // bool operator()(const GEdge& e1, const GEdge& e2) {
    //     if (e1.top == e2.top) {
    //         return e1.bot < e2.bot;
    //     } else return e1.top < e2.top;
    // }
    // /**
    //  * @brief Compare two edges first by Y value then by X value.
    //  * Highest-ranked edge should be located at top left.
    //  * @param e1 Edge 1.
    //  * @param e2 Edge 2.
    //  * @return true if Edge 1 is ranked higher then Edge 2.
    //  * @return false if otherwise.
    //  */
    // bool operator()(const GEdge& e1, const GEdge& e2){
    //     if (e1.ITopY == e2.ITopY) {
    //         if (e1.IBotY == e2.IBotY) {
    //             if (e1.ITopX == e2.ITopX) return e1.IBotX < e2.IBotX;
    //             else return e1.ITopX < e2.ITopX;
    //         } else return e1.IBotY < e2.IBotY;
    //     } else return e1.ITopY < e2.ITopY;
    // }
};