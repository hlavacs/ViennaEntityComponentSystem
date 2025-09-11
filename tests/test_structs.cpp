struct Position {
    float x{0.0f};
    float y{0.0f};

    Position() = default;
    Position(float a, float b) {
        x = a;
        y = b;
    }
    Position(const Position& pos) : Position(pos.x, pos.y) {}

    bool operator==(const Position& rp) const {
        return (x == rp.x && y == rp.y);
    }
};

struct Velocity {
    float x{1.0f};
    float y{1.0f};

    Velocity() = default;
    Velocity(float a, float b) {
        x = a;
        y = b;
    }
    Velocity(const Velocity& vel) : Velocity(vel.x, vel.y) {}
};

struct Sprite {
    char name{' '};
};